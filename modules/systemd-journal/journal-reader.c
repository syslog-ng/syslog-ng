/*
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
#include "ack_tracker.h"
#include "parse-number.h"
#include "journal-reader.h"
#include "timeutils/misc.h"

#include <stdlib.h>
#include <iv_event.h>

#define JR_THREADED 0x0001

#define MAX_CURSOR_LENGTH 1024

#define DEFAULT_FIELD_SIZE (64 * 1024)
#define DEFAULT_PRIO (LOG_LOCAL0 | LOG_NOTICE)
#define DEFAULT_FETCH_LIMIT 10

static gboolean journal_reader_initialized = FALSE;

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
  Journald *journal;
  PollEvents *poll_events;
  struct iv_event schedule_wakeup;
  struct iv_task restart_task;
  MainLoopIOWorkerJob io_job;
  gboolean watches_running:1, suspended:1;
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

static void
_map_key_value_pairs_to_syslog_macros(LogMessage *msg, gchar *key, gchar *value, gssize value_len)
{
  if (strcmp(key, "MESSAGE") == 0)
    {
      log_msg_set_value(msg, LM_V_MESSAGE, value, value_len);
      msg_debug("Incoming log entry from journal",
                evt_tag_printf("message", "%.*s", (int)value_len, value));
    }
  else if (strcmp(key, "_HOSTNAME") == 0)
    {
      log_msg_set_value(msg, LM_V_HOST, value, value_len);
    }
  else if (strcmp(key, "_PID") == 0)
    {
      log_msg_set_value(msg, LM_V_PID, value, value_len);
    }
  else if (strcmp(key, "SYSLOG_FACILITY") == 0)
    {
      msg->pri = (msg->pri & 7) | atoi(value) << 3;
    }
  else if (strcmp(key, "PRIORITY") == 0)
    {
      msg->pri = (msg->pri & ~7) | atoi(value);
    }
}

static void
_format_value_name_with_prefix(gchar *buf, gsize buf_len, JournalReaderOptions *options, const gchar *key)
{
  gsize cont = 0;

  if (options->prefix)
    cont = g_strlcpy(buf, options->prefix, buf_len);
  g_strlcpy(buf + cont, key, buf_len - cont);
}

static void
_set_value_in_message(JournalReaderOptions *options, LogMessage *msg, gchar *key, gchar *value, gssize value_len)
{
  gchar name_with_prefix[256];

  _format_value_name_with_prefix(name_with_prefix, sizeof(name_with_prefix), options, key);
  log_msg_set_value_by_name(msg, name_with_prefix, value, value_len);
}

static const gchar *
_get_value_from_message(JournalReaderOptions *options, LogMessage *msg,  const gchar *key, gssize *value_length)
{
  gchar name_with_prefix[256];

  _format_value_name_with_prefix(name_with_prefix, sizeof(name_with_prefix), options, key);
  return log_msg_get_value_by_name(msg, name_with_prefix, value_length);
}

static void
_handle_data(gchar *key, gchar *value, gpointer user_data)
{
  gpointer *args = user_data;

  LogMessage *msg = args[0];
  JournalReaderOptions *options = args[1];
  gssize value_len = MIN(strlen(value), options->max_field_size);

  _map_key_value_pairs_to_syslog_macros(msg, key, value, value_len);
  _set_value_in_message(options, msg, key, value, value_len);
}

static void
_set_program(JournalReaderOptions *options, LogMessage *msg)
{
  gssize value_length = 0;
  const gchar *value_ref = _get_value_from_message(options, msg, "SYSLOG_IDENTIFIER", &value_length);

  if (value_length <= 0)
    {
      value_ref = _get_value_from_message(options, msg, "_COMM", &value_length);
    }

  /* we need to strdup the value_ref: referred value can change during log_msg_set_value if nvtable realloc needed */
  gchar *value = g_strdup(value_ref);
  log_msg_set_value(msg, LM_V_PROGRAM, value, value_length);
  g_free(value);
}

static void
_set_message_timestamp(JournalReader *self, LogMessage *msg)
{
  guint64 ts;

  journald_get_realtime_usec(self->journal, &ts);
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

  log_source_post(&self->super, msg);
  return log_source_free_to_send(&self->super);
}

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
  gint rc = journald_seek_head(self->journal);
  if (rc != 0)
    {
      msg_error("Failed to seek to the start position of the journal",
                evt_tag_errno("error", -rc));
      return FALSE;
    }
  else
    {
      msg_debug("Seeking the journal to the start position");
    }
  return TRUE;
}

static gboolean
_seek_to_tail(JournalReader *self)
{
  gint rc = journald_seek_tail(self->journal);

  if (rc != 0)
    {
      msg_error("Failed to seek to the end position of the journal",
                evt_tag_errno("error", -rc));
      return FALSE;
    }

  msg_debug("Seeking to the journal to the end position");

  return TRUE;
}

static gboolean
_skip_old_records(JournalReader *self)
{
  if (!_seek_to_tail(self))
    return FALSE;
  if (journald_next(self->journal) <= 0)
    {
      msg_error("Can't move cursor to the next position after seek to tail.");
      return FALSE;
    }
  return TRUE;
}

static gboolean
_journal_seek(Journald *journal, const gchar *cursor)
{
  gint rc = journald_seek_cursor(journal, cursor);

  if (rc != 0)
    {
      msg_warning("Failed to seek journal to the saved cursor position",
                  evt_tag_str("cursor", cursor),
                  evt_tag_errno("error", -rc));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_journal_next(Journald *journal)
{
  gint rc = journald_next(journal);

  if (rc != 1)
    {
      msg_warning("Failed to step cursor",
                  evt_tag_errno("error", -rc));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_journal_test_cursor(Journald *journal, const gchar *cursor)
{
  gint rc = journald_test_cursor(journal, cursor);

  if (rc <= 0)
    {
      msg_warning("Current position not matches to the saved cursor position, seek to head",
                  evt_tag_str("cursor", cursor),
                  evt_tag_errno("error", -rc));
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

  msg_debug("Seeking the journal to the last cursor position",
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
          msg_error("JournalReader: Failed to allocate state");
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
  journald_get_cursor(self->journal, &cursor);
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
      gint rc = journald_next(self->journal);
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
_work_finished(gpointer s)
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
_work_perform(gpointer s, GIOCondition cond)
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
      main_loop_io_worker_job_submit(&self->io_job, G_IO_IN);
    }
  else
    {
      if (!main_loop_worker_job_quit())
        {
          _work_perform(s, G_IO_IN);
          _work_finished(s);
        }
    }
}

static void
_io_process_async_input(gpointer s)
{
  JournalReader *self = (JournalReader *)s;
  journald_process(self->journal);
  _io_process_input(s);
}

static gboolean
_add_poll_events(JournalReader *self)
{
  gint fd = journald_get_fd(self->journal);
  if (fd < 0)
    {
      msg_error("Error setting up journal polling, journald_get_fd() returned failure",
                evt_tag_errno("error", -fd));
      journald_close(self->journal);
      return FALSE;
    }

  self->poll_events = poll_fd_events_new(fd);
  poll_events_set_callback(self->poll_events, _io_process_async_input, self);
  return TRUE;
}

static gboolean
_init(LogPipe *s)
{
  JournalReader *self = (JournalReader *)s;

  if (journal_reader_initialized)
    {
      msg_error("The configuration must not contain more than one systemd-journal() source");
      return FALSE;
    }

  if (!log_source_init(s))
    return FALSE;

  gint res = journald_open(self->journal, SD_JOURNAL_LOCAL_ONLY);
  if (res < 0)
    {
      msg_error("Error opening the journal",
                evt_tag_errno("error", -res));
      return FALSE;
    }

  if (!_set_starting_position(self))
    {
      journald_close(self->journal);
      return FALSE;
    }

  if (!_add_poll_events(self))
    {
      return FALSE;
    }

  self->immediate_check = TRUE;
  journal_reader_initialized = TRUE;
  _update_watches(self);
  iv_event_register(&self->schedule_wakeup);
  return TRUE;
}

static gboolean
_deinit(LogPipe *s)
{
  JournalReader *self = (JournalReader *)s;
  _stop_watches(self);
  journald_close(self->journal);
  poll_events_free(self->poll_events);
  journal_reader_initialized = FALSE;
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

void
journal_reader_set_options(LogPipe *s, LogPipe *control, JournalReaderOptions *options,
                           const gchar *stats_id, const gchar *stats_instance)
{
  JournalReader *self = (JournalReader *) s;

  log_source_set_options(&self->super, &options->super, stats_id, stats_instance,
                         (options->flags & JR_THREADED), TRUE, control->expr_node);

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
  self->io_job.work = (void (*)(void *, GIOCondition)) _work_perform;
  self->io_job.completion = (void (*)(void *)) _work_finished;
  self->io_job.engage = (void (*)(void *)) log_pipe_ref;
  self->io_job.release = (void (*)(void *)) log_pipe_unref;
}

JournalReader *
journal_reader_new(GlobalConfig *cfg, Journald *journal)
{
  JournalReader *self = g_new0(JournalReader, 1);
  log_source_init_instance(&self->super, cfg);
  self->super.wakeup = _reader_wakeup;
  self->super.super.init = _init;
  self->super.super.deinit = _deinit;
  self->super.super.free_fn = _free;
  self->persist_name = g_strdup("systemd-journal");
  self->journal = journal;
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
  options->initialized = TRUE;
}

void
journal_reader_options_defaults(JournalReaderOptions *options)
{
  log_source_options_defaults(&options->super);
  options->super.stats_level = STATS_LEVEL0;
  options->super.stats_source = SCS_JOURNALD;
  options->fetch_limit = DEFAULT_FETCH_LIMIT;
  options->default_pri = DEFAULT_PRIO;
  options->max_field_size = DEFAULT_FIELD_SIZE;
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
  options->initialized = FALSE;
}
