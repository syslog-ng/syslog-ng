/*
 * Copyright (c) 2023 One Identity LLC.
 * Copyright (c) 2010-2014 Balabit
 * Copyright (c) 2010-2014 Viktor Juhasz <viktor.juhasz@balabit.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "syslog-ng.h"
#include "logmsg/logmsg.h"
#include "logpipe.h"
#include "messages.h"
#include "poll-fd-events.h"
#include "mainloop-io-worker.h"
#include "persist-state.h"
#include "persistable-state-header.h"
#include "ack-tracker/ack_tracker.h"
#include "parse-number.h"
#include "journal-reader.h"
#include "timeutils/cache.h"
#include "ack-tracker/ack_tracker_factory.h"
#include "string-list.h"

#include <stdlib.h>
#include <iv_event.h>

#define JR_THREADED 0x0001

#define MAX_CURSOR_LENGTH 1024

#define DEFAULT_FIELD_SIZE (64 * 1024)
#define DEFAULT_PRIO (LOG_LOCAL0 | LOG_NOTICE)
#define DEFAULT_FETCH_LIMIT 10


#if SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
GList *used_namespaces = NULL;
#else
static gboolean journal_reader_initialized = FALSE;
#endif

typedef struct _JournalReaderState
{
  PersistableStateHeader header;
  gchar cursor[MAX_CURSOR_LENGTH];
} JournalReaderState;

typedef struct _JournalBookmarkData
{
  PersistEntryHandle persist_handle;
  gchar *cursor;
} JournalBookmarkData;

struct _JournalReader
{
  LogSource super;
  LogPipe *control;
  JournalReaderOptions *options;
  sd_journal *journal;
  PollEvents *poll_events;
  struct iv_event schedule_wakeup;
  struct iv_task restart_task;
  MainLoopIOWorkerJob io_job;
  guint watches_running:1, suspended:1;
  gint notify_code;
  gboolean immediate_check;

  PersistState *persist_state;
  PersistEntryHandle persist_handle;
  gchar *persist_name;
};

static void
_start_watches_if_stopped(JournalReader *self)
{
  if (!self->watches_running)
    {
      poll_events_start_watches(self->poll_events);
      self->watches_running = TRUE;
    }
}

static void
_suspend_until_awoken(JournalReader *self)
{
  self->immediate_check = FALSE;
  poll_events_suspend_watches(self->poll_events);
  self->suspended = TRUE;
}

static void
_force_check_in_next_poll(JournalReader *self)
{
  self->immediate_check = FALSE;
  poll_events_suspend_watches(self->poll_events);
  self->suspended = FALSE;

  if (!iv_task_registered(&self->restart_task))
    {
      iv_task_register(&self->restart_task);
    }
}

static void
_update_watches(JournalReader *self)
{
  gboolean free_to_send;

  main_loop_assert_main_thread();

  _start_watches_if_stopped(self);

  free_to_send = log_source_free_to_send(&self->super);
  if (!free_to_send)
    {
      _suspend_until_awoken(self);
      return;
    }

  if (self->immediate_check)
    {
      _force_check_in_next_poll(self);
      return;
    }
  poll_events_update_watches(self->poll_events, G_IO_IN);
}

static void
_wakeup_triggered(gpointer s)
{
  JournalReader *self = (JournalReader *) s;

  if (!self->io_job.working && self->suspended)
    {
      self->immediate_check = TRUE;
      _update_watches(self);
    }
}

static void
_reader_wakeup(LogSource *s)
{
  JournalReader *self = (JournalReader *) s;

  if (self->super.super.flags & PIF_INITIALIZED)
    iv_event_post(&self->schedule_wakeup);
}


static inline gboolean
_key_matches(const gchar *key, gsize key_len, const gchar *exp)
{
  gsize exp_len = strlen(exp);
  if (key_len != exp_len)
    return FALSE;

  if (strncmp(key, exp, key_len) != 0)
    return FALSE;

  return TRUE;
}

static void
_map_key_value_pairs_to_syslog_macros(LogMessage *msg, gchar *key, gsize key_len, gchar *value, gssize value_len)
{
  if (_key_matches(key, key_len, "MESSAGE"))
    {
      log_msg_set_value(msg, LM_V_MESSAGE, value, value_len);
    }
  else if (_key_matches(key, key_len, "_HOSTNAME"))
    {
      log_msg_set_value(msg, LM_V_HOST, value, value_len);
    }
  else if (_key_matches(key, key_len, "_PID"))
    {
      log_msg_set_value(msg, LM_V_PID, value, value_len);
    }
  else if (_key_matches(key, key_len, "SYSLOG_FACILITY"))
    {
      msg->pri = (msg->pri & 7) | atoi(value) << 3;
    }
  else if (_key_matches(key, key_len, "PRIORITY"))
    {
      msg->pri = (msg->pri & ~7) | atoi(value);
    }
}

static void
_format_value_name_with_prefix(gchar *buf, gsize buf_len,
                               JournalReaderOptions *options,
                               const gchar *key, gssize key_len)
{
  gsize cont = 0;

  if (key_len < 0)
    key_len = strlen(key);

  if (options->prefix)
    cont = g_strlcpy(buf, options->prefix, buf_len);
  gsize left = buf_len - cont;
  if (left >= key_len + 1)
    {
      strncpy(buf + cont, key, key_len);
      buf[cont + key_len] = 0;
    }
  else
    {
      g_strlcpy(buf + cont, key, buf_len - cont);
    }
}

static void
_set_value_in_message(JournalReaderOptions *options, LogMessage *msg,
                      gchar *key, gsize key_len, gchar *value, gssize value_len)
{
  gchar name_with_prefix[256];

  _format_value_name_with_prefix(name_with_prefix, sizeof(name_with_prefix), options, key, key_len);
  log_msg_set_value_by_name(msg, name_with_prefix, value, value_len);
}

static const gchar *
_get_value_from_message(JournalReaderOptions *options, LogMessage *msg,
                        const gchar *key, gssize key_len, gssize *value_length)
{
  gchar name_with_prefix[256];

  _format_value_name_with_prefix(name_with_prefix, sizeof(name_with_prefix), options, key, key_len);
  return log_msg_get_value_by_name(msg, name_with_prefix, value_length);
}

static void
_handle_data(gchar *key, gsize key_len, gchar *value, gsize value_len, gpointer user_data)
{
  gpointer *args = user_data;

  LogMessage *msg = args[0];
  JournalReaderOptions *options = args[1];
  value_len = MIN(value_len, options->max_field_size);

  _map_key_value_pairs_to_syslog_macros(msg, key, key_len, value, value_len);
  _set_value_in_message(options, msg, key, key_len, value, value_len);
}

static void
_set_program(JournalReaderOptions *options, LogMessage *msg)
{
  gssize value_length = 0;
  const gchar *value_ref = _get_value_from_message(options, msg, "SYSLOG_IDENTIFIER", -1, &value_length);

  if (value_length <= 0)
    {
      value_ref = _get_value_from_message(options, msg, "_COMM", -1, &value_length);
    }

  /* we need to reference the payload: referred value can change during log_msg_set_value if nvtable realloc needed */
  NVTable *nvtable = nv_table_ref(msg->payload);
  log_msg_set_value(msg, LM_V_PROGRAM, value_ref, value_length);
  nv_table_unref(nvtable);
}

static void
_set_transport(LogMessage *msg)
{
  log_msg_set_value_to_string(msg, LM_V_TRANSPORT, "local+journal");
}

static void
_set_message_timestamp(JournalReader *self, LogMessage *msg)
{
  guint64 ts;

  sd_journal_get_realtime_usec(self->journal, &ts);
  msg->timestamps[LM_TS_STAMP].ut_sec = ts / 1000000;
  msg->timestamps[LM_TS_STAMP].ut_usec = ts % 1000000;
  msg->timestamps[LM_TS_STAMP].ut_gmtoff = time_zone_info_get_offset(self->options->recv_time_zone_info,
                                           msg->timestamps[LM_TS_STAMP].ut_sec);
  if (msg->timestamps[LM_TS_STAMP].ut_gmtoff == -1)
    {
      msg->timestamps[LM_TS_STAMP].ut_gmtoff = get_local_timezone_ofs(msg->timestamps[LM_TS_STAMP].ut_sec);
    }
}

static gboolean
_handle_message(JournalReader *self)
{
  LogMessage *msg = log_msg_new_empty();

  msg->pri = self->options->default_pri;

  gpointer args[] = {msg, self->options};

  journald_foreach_data(self->journal, _handle_data, args);

  _set_message_timestamp(self, msg);
  _set_program(self->options, msg);
  _set_transport(msg);

  msg_debug("Incoming log entry from journal",
            evt_tag_printf("input", "%s", log_msg_get_value(msg, LM_V_MESSAGE, NULL)),
            evt_tag_msg_reference(msg));

  log_source_post(&self->super, msg);
  return log_source_free_to_send(&self->super);
}

#if SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
static gboolean
_namespace_is_already_in_use(JournalReader *self, const gchar *namespace)
{
  GList *item = g_list_find_custom(used_namespaces, namespace, (GCompareFunc)strcmp);
  if (item == NULL)
    {
      used_namespaces = g_list_prepend(used_namespaces, (gpointer)namespace);
      return FALSE;
    }

  msg_error("systemd-journal namespace already in use", evt_tag_str("namespace", namespace));
  return TRUE;
}
#endif

static gboolean
_alloc_state(JournalReader *self)
{
  self->persist_handle = persist_state_alloc_entry(self->persist_state, self->persist_name, sizeof(JournalReaderState));
  JournalReaderState *state = persist_state_map_entry(self->persist_state, self->persist_handle);
  if (!state)
    return FALSE;

  state->header.version = 0;
  state->header.big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);

  persist_state_unmap_entry(self->persist_state, self->persist_handle);
  return TRUE;
}

static gboolean
_load_state(JournalReader *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super);

  gsize state_size;
  guint8 persist_version;

  self->persist_state = cfg->state;
  self->persist_handle = persist_state_lookup_entry(self->persist_state, self->persist_name, &state_size,
                                                    &persist_version);
  return !!(self->persist_handle);
}

static inline gboolean
_seek_to_head(JournalReader *self)
{
  gint rc = sd_journal_seek_head(self->journal);
  if (rc < 0)
    {
      msg_error("systemd-journal: Failed to seek to the start position of the journal",
                evt_tag_errno("error", -rc));
      return FALSE;
    }
  else
    {
      msg_debug("systemd-journal: Seeking journal to the start position");
    }
  return TRUE;
}

static gboolean
_seek_to_tail(JournalReader *self)
{
  gint rc = sd_journal_seek_tail(self->journal);

  if (rc < 0)
    {
      msg_error("systemd-journal: Failed to seek to the end position of the journal",
                evt_tag_errno("error", -rc));
      return FALSE;
    }

  msg_debug("systemd-journal: Seeking journal to the end position");

  return TRUE;
}

static gboolean
_skip_old_records(JournalReader *self)
{
  if (!_seek_to_tail(self))
    return FALSE;
  int rc = sd_journal_next(self->journal);
  if (rc < 0)
    {
      msg_error("systemd-journal: Error processing read-old-records(no), sd_journal_next() failed after sd_journal_seek_tail()",
                evt_tag_errno("error", -rc));
      return FALSE;
    }
  return TRUE;
}

static gboolean
_journal_seek(sd_journal *journal, const gchar *cursor)
{
  gint rc = sd_journal_seek_cursor(journal, cursor);

  if (rc < 0)
    {
      msg_warning("systemd-journal: Failed to seek journal to the saved cursor position",
                  evt_tag_str("cursor", cursor),
                  evt_tag_errno("error", -rc));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_journal_next(sd_journal *journal)
{
  gint rc = sd_journal_next(journal);

  if (rc != 1)
    {
      msg_warning("systemd-journal: Failed to step cursor",
                  evt_tag_int("rc", rc),
                  evt_tag_errno("error", rc < 0 ? -rc : 0));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_journal_test_cursor(sd_journal *journal, const gchar *cursor)
{
  gint rc = sd_journal_test_cursor(journal, cursor);

  if (rc <= 0)
    {
      msg_warning("systemd-journal: Current position does not match the previously restored cursor position, seeking to head",
                  evt_tag_str("cursor", cursor),
                  evt_tag_int("rc", rc),
                  evt_tag_errno("error", rc < 0 ? -rc : 0));
      return FALSE;
    }

  return TRUE;
}

static inline gboolean
_seek_to_saved_state(JournalReader *self)
{
  JournalReaderState *state = persist_state_map_entry(self->persist_state, self->persist_handle);

  if (!_journal_seek(self->journal, state->cursor) ||
      !_journal_next(self->journal) ||
      !_journal_test_cursor(self->journal, state->cursor))
    {
      persist_state_unmap_entry(self->persist_state, self->persist_handle);

      return _seek_to_head(self);
    }

  msg_debug("systemd-journal: Seeking the journal to the last cursor position",
            evt_tag_str("cursor", state->cursor));

  persist_state_unmap_entry(self->persist_state, self->persist_handle);

  return TRUE;
}

static gboolean
_set_starting_position(JournalReader *self)
{
  if (!_load_state(self))
    {
      if (!_alloc_state(self))
        {
          msg_error("systemd-journal: Failed to allocate persistent state");
          return FALSE;
        }
      if (self->super.options->read_old_records)
        return _seek_to_head(self);
      return _skip_old_records(self);
    }
  return _seek_to_saved_state(self);
}

static gchar *
_get_cursor(JournalReader *self)
{
  gchar *cursor;
  sd_journal_get_cursor(self->journal, &cursor);
  return cursor;
}

static void
_reader_save_state(Bookmark *bookmark)
{
  JournalBookmarkData *bookmark_data = (JournalBookmarkData *)(&bookmark->container);
  JournalReaderState *state = persist_state_map_entry(bookmark->persist_state, bookmark_data->persist_handle);
  strcpy(state->cursor, bookmark_data->cursor);
  persist_state_unmap_entry(bookmark->persist_state, bookmark_data->persist_handle);
}

static void
_destroy_bookmark(Bookmark *bookmark)
{
  JournalBookmarkData *bookmark_data = (JournalBookmarkData *)(&bookmark->container);
  free(bookmark_data->cursor);
}

static void
_fill_bookmark(JournalReader *self, Bookmark *bookmark)
{
  JournalBookmarkData *bookmark_data = (JournalBookmarkData *)(&bookmark->container);
  bookmark_data->cursor = _get_cursor(self);
  bookmark_data->persist_handle = self->persist_handle;
  bookmark->save = _reader_save_state;
  bookmark->destroy = _destroy_bookmark;
}

static gint
_fetch_log(JournalReader *self)
{
  gint msg_count = 0;
  gint result = 0;
  self->immediate_check = TRUE;
  while (msg_count < self->options->fetch_limit && !main_loop_worker_job_quit())
    {
      gint rc = sd_journal_next(self->journal);
      if (rc > 0)
        {
          Bookmark *bookmark = ack_tracker_request_bookmark(self->super.ack_tracker);
          _fill_bookmark(self, bookmark);
          msg_count++;
          if (!_handle_message(self))
            {
              break;
            }
        }
      else
        {
          self->immediate_check = FALSE;
          /* rc == 0 means EOF */
          if (rc < 0)
            {
              msg_error("Error occurred while getting next message from journal",
                        evt_tag_errno("error", -rc));
              result = NC_READ_ERROR;
            }
          break;
        }
    }
  return result;
}

static void
_work_finished(gpointer s, gpointer arg)
{
  JournalReader *self = (JournalReader *) s;
  if (self->notify_code)
    {
      gint notify_code = self->notify_code;

      self->notify_code = 0;
      log_pipe_notify(self->control, notify_code, self);
    }
  if (self->super.super.flags & PIF_INITIALIZED)
    {
      _update_watches(self);
    }
}

static void
_work_perform(gpointer s, gpointer arg)
{
  JournalReader *self = (JournalReader *) s;
  self->notify_code = _fetch_log(self);
}

static void
_stop_watches(JournalReader *self)
{
  if (self->watches_running)
    {
      poll_events_stop_watches(self->poll_events);

      if (iv_task_registered(&self->restart_task))
        iv_task_unregister(&self->restart_task);
      self->watches_running = FALSE;
    }
}

static void
_io_process_input(gpointer s)
{
  JournalReader *self = (JournalReader *) s;

  _stop_watches(self);
  if ((self->options->flags & JR_THREADED))
    {
      main_loop_io_worker_job_submit(&self->io_job, NULL);
    }
  else
    {
      if (!main_loop_worker_job_quit())
        {
          log_pipe_ref(&self->super.super);
          _work_perform(s, NULL);
          _work_finished(s, NULL);
          log_pipe_unref(&self->super.super);
        }
    }
}

static void
_io_process_async_input(gpointer s)
{
  JournalReader *self = (JournalReader *)s;
  sd_journal_process(self->journal);
  _io_process_input(s);
}

static gboolean
_add_poll_events(JournalReader *self)
{
  gint fd = sd_journal_get_fd(self->journal);
  if (fd < 0)
    {
      msg_error("Error setting up journal polling, sd_journal_get_fd() returned failure",
                evt_tag_errno("error", -fd));
      sd_journal_close(self->journal);
      return FALSE;
    }

  self->poll_events = poll_fd_events_new(fd);
  poll_events_set_callback(self->poll_events, _io_process_async_input, self);
  return TRUE;
}

#if SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
static void
_journal_process_namespace(gchar *namespace_option, gchar **journal_namespace, gint *journal_flags)
{
  if (strcmp(namespace_option, "*") == 0)
    {
      *journal_flags |= SD_JOURNAL_ALL_NAMESPACES;
      if (strlen(namespace_option) > 1)
        {
          msg_warning("namespace('*'): discarding everything after the asterisk");
        }
    }
  else if (strncmp(namespace_option, "+", 1) == 0)
    {
      *journal_flags |= SD_JOURNAL_INCLUDE_DEFAULT_NAMESPACE;
      if (strlen(namespace_option) > 1)
        {
          *journal_namespace = namespace_option + 1;
        }
    }
  else
    {
      if (strlen(namespace_option) > 0)
        {
          *journal_namespace = namespace_option;
        }
    }
}
#endif

static gint
_journal_open(JournalReader *self)
{
  gint journal_flags = SD_JOURNAL_LOCAL_ONLY;

#if SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
  gchar *journal_namespace = NULL;
  _journal_process_namespace(self->options->namespace, &journal_namespace, &journal_flags);
  return sd_journal_open_namespace(&self->journal, journal_namespace, journal_flags);
#else
  return sd_journal_open(&self->journal, journal_flags);
#endif
}

static gboolean
_journal_apply_matches_list(JournalReader *self)
{
  gint rc;

  for (GList *l = self->options->matches; l && l->next; l = l->next->next)
    {
      const gchar *field = l->data;
      const gchar *value = l->next->data;
      gchar *match_expr;

      match_expr = g_strdup_printf("%s=%s", field, value);
      rc = sd_journal_add_match(self->journal, match_expr, 0);
      if (rc < 0)
        {
          msg_error("Error applying filtering matches to systemd-journal()",
                    evt_tag_str("match_expr", match_expr),
                    evt_tag_str("error", g_strerror(-rc)));
          g_free(match_expr);
          return FALSE;
        }
      g_free(match_expr);
    }
  return TRUE;
}

static gboolean
_journal_apply_match_boot(JournalReader *self)
{
  if (self->options->match_boot)
    {
      gint rc = journald_filter_this_boot(self->journal);
      if (rc < 0)
        {
          msg_error("Error applying the filter to the current boot",
                    evt_tag_str("error", g_strerror(-rc)));
        }
    }
  return TRUE;
}

static gint
_journal_apply_matches(JournalReader *self)
{
  if (!_journal_apply_matches_list(self))
    return FALSE;
  if (!_journal_apply_match_boot(self))
    return FALSE;
  return TRUE;
}

static gboolean
_init_persist_name(JournalReader *self)
{
#if SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
  if (strcmp(self->options->namespace, "*") != 0)
    {
      self->persist_name = g_strdup_printf("systemd_journal(%s)", self->options->namespace);
    }
  else
    {
      self->persist_name = g_strdup("systemd-journal");
    }
  return !_namespace_is_already_in_use(self, self->options->namespace);
#endif
  self->persist_name = g_strdup("systemd-journal");
  return TRUE;
}

static gboolean
_init(LogPipe *s)
{
  JournalReader *self = (JournalReader *)s;

#ifndef SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
  if (journal_reader_initialized)
    {
      msg_error("The configuration must not contain more than one systemd-journal() source");
      return FALSE;
    }
#endif

  if (! _init_persist_name(self))
    {
      msg_error("The configuration must not contain more than one systemd-journal() source with the same namespace() option");
      return FALSE;
    }

  if (!log_source_init(s))
    return FALSE;

  gint res = _journal_open(self);
  if (res < 0)
    {
      msg_error("Error opening the journal",
                evt_tag_errno("error", -res));
      return FALSE;
    }
  if (!_journal_apply_matches(self))
    {
      sd_journal_close(self->journal);
      return FALSE;
    }

  if (!_set_starting_position(self))
    {
      sd_journal_close(self->journal);
      return FALSE;
    }

  if (!_add_poll_events(self))
    {
      return FALSE;
    }

  self->immediate_check = TRUE;
#ifndef SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
  journal_reader_initialized = TRUE;
#endif
  _update_watches(self);
  iv_event_register(&self->schedule_wakeup);
  return TRUE;
}

static gboolean
_deinit(LogPipe *s)
{
  JournalReader *self = (JournalReader *)s;

#if SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
  GList *link = g_list_find(used_namespaces, self->options->namespace);
  if (link) used_namespaces = g_list_delete_link(used_namespaces, link);
#else
  journal_reader_initialized = FALSE;
#endif

  _stop_watches(self);
  sd_journal_close(self->journal);
  poll_events_free(self->poll_events);
  return TRUE;
}

static void
_free(LogPipe *s)
{
  JournalReader *self = (JournalReader *) s;
  log_pipe_unref(self->control);
  log_source_free(&self->super.super);
  g_free(self->persist_name);
  return;
}

sd_journal *
journal_reader_get_sd_journal(JournalReader *self)
{
  return self->journal;
}

void
journal_reader_set_options(LogPipe *s, LogPipe *control, JournalReaderOptions *options,
                           const gchar *stats_id, StatsClusterKeyBuilder *kb)
{
  JournalReader *self = (JournalReader *) s;

  log_source_set_options(&self->super, &options->super, stats_id, kb,
                         (options->flags & JR_THREADED), control->expr_node);
  log_source_set_ack_tracker_factory(&self->super, consecutive_ack_tracker_factory_new());

  log_pipe_unref(self->control);
  log_pipe_ref(control);
  self->control = control;
  self->options = options;
}

static void
_init_watches(JournalReader *self)
{
  IV_EVENT_INIT(&self->schedule_wakeup);
  self->schedule_wakeup.cookie = self;
  self->schedule_wakeup.handler = _wakeup_triggered;
  iv_event_register(&self->schedule_wakeup);

  IV_TASK_INIT(&self->restart_task);
  self->restart_task.cookie = self;
  self->restart_task.handler = _io_process_input;

  main_loop_io_worker_job_init(&self->io_job);
  self->io_job.user_data = self;
  self->io_job.work = _work_perform;
  self->io_job.completion = _work_finished;
  self->io_job.engage = (void (*)(void *)) log_pipe_ref;
  self->io_job.release = (void (*)(void *)) log_pipe_unref;
}

JournalReader *
journal_reader_new(GlobalConfig *cfg)
{
  JournalReader *self = g_new0(JournalReader, 1);
  log_source_init_instance(&self->super, cfg);
  self->super.wakeup = _reader_wakeup;
  self->super.super.init = _init;
  self->super.super.deinit = _deinit;
  self->super.super.free_fn = _free;
  self->persist_name = NULL;
  _init_watches(self);
  return self;
}

void
journal_reader_options_init(JournalReaderOptions *options, GlobalConfig *cfg, const gchar *group_name)
{
  if (options->initialized)
    return;

  log_source_options_init(&options->super, cfg, group_name);
  if (cfg->threaded)
    options->flags |= JR_THREADED;

  if (options->recv_time_zone == NULL)
    options->recv_time_zone = g_strdup(cfg->recv_time_zone);
  if (options->recv_time_zone_info == NULL)
    options->recv_time_zone_info = time_zone_info_new(options->recv_time_zone);


  if (options->prefix == NULL)
    {
      const gchar *default_prefix = ".journald.";
      if (cfg_is_config_version_older(cfg, VERSION_VALUE_3_8))
        {
          msg_warning("WARNING: Default value changed for the prefix() option of systemd-journal source in " VERSION_3_8,
                      evt_tag_str("old_value", ""),
                      evt_tag_str("new_value", default_prefix));
        }
      else
        {
          options->prefix = g_strdup(default_prefix);
        }
    }
#if SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
  if (options->namespace == NULL)
    {
      const gchar *default_namespace = "*";
      options->namespace = g_strdup(default_namespace);
    }
#else
  if (options->namespace)
    {
      msg_warning("namespace() option on systemd-journal() will have no effect! (systemd < v245)");
    }
#endif
  options->initialized = TRUE;
}

void
journal_reader_options_set_default_severity(JournalReaderOptions *self, gint severity)
{
  if (self->default_pri == 0xFFFF)
    self->default_pri = LOG_USER;
  self->default_pri = (self->default_pri & ~7) | severity;
}

void
journal_reader_options_set_default_facility(JournalReaderOptions *self, gint facility)
{
  if (self->default_pri == 0xFFFF)
    self->default_pri = LOG_NOTICE;
  self->default_pri = (self->default_pri & 7) | facility;
}

void
journal_reader_options_set_time_zone(JournalReaderOptions *self, gchar *time_zone)
{
  if (self->recv_time_zone)
    g_free(self->recv_time_zone);
  self->recv_time_zone = g_strdup(time_zone);
}

void
journal_reader_options_set_prefix(JournalReaderOptions *self, gchar *prefix)
{
  if (self->prefix)
    g_free(self->prefix);
  self->prefix = g_strdup(prefix);
}

void
journal_reader_options_set_max_field_size(JournalReaderOptions *self, gint max_field_size)
{
  self->max_field_size = max_field_size;
}

void
journal_reader_options_set_namespace(JournalReaderOptions *self, gchar *namespace)
{
  if (self->namespace)
    g_free(self->namespace);
  self->namespace = g_strdup(namespace);
}

void
journal_reader_options_set_log_fetch_limit(JournalReaderOptions *self, gint log_fetch_limit)
{
  self->fetch_limit = log_fetch_limit;
}

void
journal_reader_options_set_matches(JournalReaderOptions *self, GList *matches)
{
  string_list_free(self->matches);
  self->matches = matches;
}

void
journal_reader_options_set_match_boot(JournalReaderOptions *self, gboolean enable)
{
  self->match_boot = enable;
}

void
journal_reader_options_defaults(JournalReaderOptions *options)
{
  log_source_options_defaults(&options->super);
  options->super.stats_level = STATS_LEVEL0;
  options->super.stats_source = stats_register_type("journald");
  options->fetch_limit = DEFAULT_FETCH_LIMIT;
  options->default_pri = DEFAULT_PRIO;
  options->max_field_size = DEFAULT_FIELD_SIZE;
  options->match_boot = FALSE;
  options->super.read_old_records = TRUE;
}

void
journal_reader_options_destroy(JournalReaderOptions *options)
{
  log_source_options_destroy(&options->super);
  if (options->prefix)
    {
      g_free(options->prefix);
      options->prefix = NULL;
    }
  if (options->recv_time_zone)
    {
      g_free(options->recv_time_zone);
      options->recv_time_zone = NULL;
    }
  if (options->recv_time_zone_info)
    {
      time_zone_info_free(options->recv_time_zone_info);
      options->recv_time_zone_info = NULL;
    }
  if (options->namespace)
    {
      g_free(options->namespace);
      options->namespace = NULL;
    }
  string_list_free(options->matches);
  options->initialized = FALSE;
}
