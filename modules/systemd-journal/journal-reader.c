/*
 * Copyright (c) 2014      BalaBit S.a.r.l., Luxembourg, Luxembourg
 * Copyright (c) 2010-2014 BalaBit IT Ltd, Budapest, Hungary
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
#include "logmsg.h"
#include "logpipe.h"
#include "messages.h"
#include "poll-fd-events.h"
#include "mainloop-io-worker.h"
#include "persist-state.h"
#include "ack_tracker.h"
#include "parse-number.h"
#include "journal-reader.h"
#include <stdlib.h>

#define JR_THREADED 0x0001

#define MAX_CURSOR_LENGTH 1024

#define DEFAULT_FIELD_SIZE (64 * 1024)
#define DEFAULT_PRIO (LOG_LOCAL0 | LOG_NOTICE)
#define DEFAULT_FETCH_LIMIT 10

static gboolean journal_reader_initialized = FALSE;

typedef struct _JournalReaderState {
  BaseState header;
  gchar cursor[MAX_CURSOR_LENGTH];
} JournalReaderState;

typedef struct _JournalBookmarkData
{
  PersistEntryHandle persist_handle;
  gchar *cursor;
} JournalBookmarkData;

struct _JournalReader {
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
__start_watches_if_stopped(JournalReader *self)
{
  if (!self->watches_running)
    {
      poll_events_start_watches(self->poll_events);
      self->watches_running = TRUE;
    }
}

static void
__suspend_until_awoken(JournalReader *self)
{
  self->immediate_check = FALSE;
  poll_events_suspend_watches(self->poll_events);
  self->suspended = TRUE;
}

static void
__force_check_in_next_poll(JournalReader *self)
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
__update_watches(JournalReader *self)
{
  gboolean free_to_send;

  main_loop_assert_main_thread();

  __start_watches_if_stopped(self);

  free_to_send = log_source_free_to_send(&self->super);
  if (!free_to_send)
    {
      __suspend_until_awoken(self);
      return;
    }

  if (self->immediate_check)
    {
      __force_check_in_next_poll(self);
      return;
    }
  poll_events_update_watches(self->poll_events, G_IO_IN);
}

static void
__wakeup_triggered(gpointer s)
{
  JournalReader *self = (JournalReader *) s;

  if (!self->io_job.working && self->suspended)
    {
      self->immediate_check = TRUE;
      __update_watches(self);
    }
}

static void
__reader_wakeup(LogSource *s)
{
  JournalReader *self = (JournalReader *) s;

  if (self->super.super.flags & PIF_INITIALIZED)
    iv_event_post(&self->schedule_wakeup);
}

static void
__handle_data(gchar *key, gchar *value, gpointer user_data)
{
  gpointer *args = user_data;

  LogMessage *msg = args[0];
  JournalReaderOptions *options = args[1];
  gssize value_len = MIN(strlen(value), options->max_field_size);

  if (strcmp(key, "MESSAGE") == 0)
    {
      log_msg_set_value(msg, LM_V_MESSAGE, value, value_len);
      msg_debug("Incoming log entry from journal",
                evt_tag_printf("message", "%.*s", (int)value_len, value),
                NULL);
    }
  else if (strcmp(key, "_HOSTNAME") == 0)
    {
      log_msg_set_value(msg, LM_V_HOST, value, value_len);
    }
  else if (strcmp(key, "_PID") == 0)
    {
      log_msg_set_value(msg, LM_V_PID, value, value_len);
    }
  else if (strcmp(key, "_COMM") == 0)
    {
      log_msg_set_value(msg, LM_V_PROGRAM, value, value_len);
    }
  else if (strcmp(key, "SYSLOG_IDENTIFIER") == 0)
    {
      gssize program_length;
      (void)log_msg_get_value(msg, LM_V_PROGRAM, &program_length);
      if (program_length == 0)
        {
          log_msg_set_value(msg, LM_V_PROGRAM, value, value_len);
        }
    }
  else if (strcmp(key, "SYSLOG_FACILITY") == 0)
    {
      msg->pri = (msg->pri & 7) | atoi(value) << 3;
    }
  else if (strcmp(key, "PRIORITY") == 0)
    {
      msg->pri = (msg->pri & ~7) | atoi(value);
    }
  else
    {
      if (!options->prefix)
        {
          NVHandle handle = log_msg_get_value_handle(key);
          log_msg_set_value(msg, handle, value, value_len);
        }
      else
        {
          gchar *prefixed_key = g_strdup_printf("%s%s", options->prefix, key);
          NVHandle prefixed_handle = log_msg_get_value_handle(prefixed_key);
          log_msg_set_value(msg, prefixed_handle, value, value_len);
          g_free(prefixed_key);
        }
    }
}

static void
__set_message_timestamp(JournalReader *self, LogMessage *msg)
{
   guint64 ts;
   journald_get_realtime_usec(self->journal, &ts);
   msg->timestamps[LM_TS_STAMP].tv_sec = ts / 1000000;
   msg->timestamps[LM_TS_STAMP].tv_usec = ts % 1000000;
   msg->timestamps[LM_TS_STAMP].zone_offset = time_zone_info_get_offset(self->options->recv_time_zone_info, msg->timestamps[LM_TS_STAMP].tv_sec);
   if (msg->timestamps[LM_TS_STAMP].zone_offset == -1)
     {
       msg->timestamps[LM_TS_STAMP].zone_offset = get_local_timezone_ofs(msg->timestamps[LM_TS_STAMP].tv_sec);
     }

}

static gboolean
__handle_message(JournalReader *self)
{
  LogMessage *msg = log_msg_new_empty();
  LogPathOptions lpo = LOG_PATH_OPTIONS_INIT;

  msg->pri = self->options->default_pri;

  gpointer args[] = {msg, self->options};

  journald_foreach_data(self->journal, __handle_data, args);
  __set_message_timestamp(self, msg);

  log_pipe_queue(&self->super.super, msg, &lpo);
  return log_source_free_to_send(&self->super);
}

static void
__alloc_state(JournalReader *self)
{
  self->persist_handle = persist_state_alloc_entry(self->persist_state, self->persist_name, sizeof(JournalReaderState));
  JournalReaderState *state = persist_state_map_entry(self->persist_state, self->persist_handle);

  state->header.version = 0;
  state->header.big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);

  persist_state_unmap_entry(self->persist_state, self->persist_handle);
}

static gboolean
__load_state(JournalReader *self, GlobalConfig *cfg)
{
  gsize state_size;
  guint8 persist_version;

  self->persist_state = cfg->state;
  self->persist_handle = persist_state_lookup_entry(self->persist_state, self->persist_name, &state_size, &persist_version);
  return !!(self->persist_handle);
}

static inline gboolean
__seek_to_head(JournalReader *self)
{
  gint rc = journald_seek_head(self->journal);
  if (rc != 0)
    {
      msg_error("Failed to seek to head of journal",
          evt_tag_errno("error", errno),
          NULL);
      return FALSE;
    }
  return TRUE;
}

static inline gboolean
__seek_to_tail(JournalReader *self)
{
  gint rc = journald_seek_tail(self->journal);
  if (rc != 0)
    {
      msg_error("Failed to seek to tail of journal",
          evt_tag_errno("error", errno),
          NULL);
      return FALSE;
    }
  return TRUE;
}

static inline gboolean
__seek_to_saved_state(JournalReader *self)
{
  JournalReaderState *state = persist_state_map_entry(self->persist_state, self->persist_handle);
  gint rc = journald_seek_cursor(self->journal, state->cursor);
  persist_state_unmap_entry(self->persist_state, self->persist_handle);
  if (rc != 0)
    {
      msg_warning("Failed to seek to the cursor",
          evt_tag_str("cursor", state->cursor),
          evt_tag_errno("error", errno),
          NULL);
      return __seek_to_head(self);
    }
  journald_next(self->journal);
  return TRUE;
}

static gboolean
__set_starting_position(JournalReader *self)
{
  if (!__load_state(self, log_pipe_get_config(&self->super.super)))
    {
      __alloc_state(self);
      return __seek_to_head(self);
    }
  return __seek_to_saved_state(self);
}

static gchar *
__get_cursor(JournalReader *self)
{
  gchar *cursor = NULL;
  journald_get_cursor(self->journal, &cursor);
  return cursor;
}

static void
__reader_save_state(Bookmark *bookmark)
{
  JournalBookmarkData *bookmark_data = (JournalBookmarkData *)(&bookmark->container);
  JournalReaderState *state = persist_state_map_entry(bookmark->persist_state, bookmark_data->persist_handle);
  strcpy(state->cursor, bookmark_data->cursor);
  persist_state_unmap_entry(bookmark->persist_state, bookmark_data->persist_handle);
}

static void
__save_current_position(JournalReader *self)
{
  JournalReaderState *state = persist_state_map_entry(self->persist_state, self->persist_handle);
  gchar *cursor = __get_cursor(self);
  strcpy(state->cursor, cursor);
  persist_state_unmap_entry(self->persist_state, self->persist_handle);
  g_free(cursor);
}

static void
__destroy_bookmark(Bookmark *bookmark)
{
  JournalBookmarkData *bookmark_data = (JournalBookmarkData *)(&bookmark->container);
  free(bookmark_data->cursor);
}

static void
__fill_bookmark(JournalReader *self, Bookmark *bookmark)
{
  JournalBookmarkData *bookmark_data = (JournalBookmarkData *)(&bookmark->container);
  bookmark_data->cursor = __get_cursor(self);
  bookmark_data->persist_handle = self->persist_handle;
  bookmark->save = __reader_save_state;
  bookmark->destroy = __destroy_bookmark;
}

gboolean
journal_reader_skip_old_messages(JournalReader *self, GlobalConfig *cfg)
{
  if (journald_open(self->journal, SD_JOURNAL_LOCAL_ONLY) < 0)
    {
      msg_error("Can't open journal",
                evt_tag_errno("error", errno),
                NULL);
      return FALSE;
    }
  if (__load_state(self, cfg))
    {
      return TRUE;
    }
  __alloc_state(self);
  if (!__seek_to_tail(self))
    {
      return FALSE;
    }
  if (journald_next(self->journal) <= 0)
    {
      msg_error("Can't get next record after seek to tail", NULL);
      return FALSE;
    }
  __save_current_position(self);
  return TRUE;
}


static gint
__fetch_log(JournalReader *self)
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
          __fill_bookmark(self, bookmark);
          msg_count++;
          if (!__handle_message(self))
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
                  evt_tag_errno("error", errno),
                  NULL);
              result = NC_READ_ERROR;
            }
          break;
        }
   }
  return result;
}

static void
__work_finished(gpointer s)
{
  JournalReader *self = (JournalReader *) s;
  if (self->notify_code)
    {
      gint notify_code = self->notify_code;

      self->notify_code = 0;
      log_pipe_notify(self->control, &self->super.super, notify_code, self);
    }
  if (self->super.super.flags & PIF_INITIALIZED)
    {
      __update_watches(self);
    }
  log_pipe_unref(&self->super.super);
}

static void
__work_perform(gpointer s)
{
  JournalReader *self = (JournalReader *) s;
  self->notify_code = __fetch_log(self);
}

static void
__stop_watches(JournalReader *self)
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
__io_process_input(gpointer s)
{
  JournalReader *self = (JournalReader *) s;

  __stop_watches(self);
  log_pipe_ref(&self->super.super);
  if ((self->options->flags & JR_THREADED))
    {
      main_loop_io_worker_job_submit(&self->io_job);
    }
  else
    {
      if (!main_loop_worker_job_quit())
        {
          __work_perform(s);
          __work_finished(s);
        }
    }
}

static void
__io_process_async_input(gpointer s)
{
  JournalReader *self = (JournalReader *)s;
  journald_process(self->journal);
  __io_process_input(s);
}

static gboolean
__add_poll_events(JournalReader *self)
{
  gint fd = journald_get_fd(self->journal);
  if (fd < 0)
    {
      msg_error("Can't get fd from journal",
          evt_tag_errno("error", errno),
          NULL);
      journald_close(self->journal);
      return FALSE;
    }

  self->poll_events = poll_fd_events_new(fd);
  poll_events_set_callback(self->poll_events, __io_process_async_input, self);
  return TRUE;
}

static gboolean
__init(LogPipe *s)
{
  JournalReader *self = (JournalReader *)s;

  if (journal_reader_initialized)
    {
      msg_error("The configuration must not contain more than one systemd-journal source",
          NULL);
      return FALSE;
    }

  if (!log_source_init(s))
    return FALSE;

  gint res = journald_open(self->journal, SD_JOURNAL_LOCAL_ONLY);
  if (res < 0)
    {
      msg_error("Can't open journal",
          evt_tag_errno("error", errno),
          NULL);
      return FALSE;
    }

  if (!__set_starting_position(self))
    {
      msg_error("Can't set starting position to the journal", NULL);
      journald_close(self->journal);
      return FALSE;
    }

  if (!__add_poll_events(self))
    {
      return FALSE;
    }

  self->immediate_check = TRUE;
  journal_reader_initialized = TRUE;
  __update_watches(self);
  iv_event_register(&self->schedule_wakeup);
  return TRUE;
}

static gboolean
__deinit(LogPipe *s)
{
  JournalReader *self = (JournalReader *)s;
  __stop_watches(self);
  journald_close(self->journal);
  poll_events_free(self->poll_events);
  journal_reader_initialized = FALSE;
  return TRUE;
}

static void
__free(LogPipe *s)
{
  JournalReader *self = (JournalReader *) s;
  log_pipe_unref(self->control);
  log_source_free(&self->super.super);
  g_free(self->persist_name);
  return;
}

void
journal_reader_set_options(LogPipe *s, LogPipe *control, JournalReaderOptions *options, gint stats_level, gint stats_source, const gchar *stats_id, const gchar *stats_instance)
{
  JournalReader *self = (JournalReader *) s;

  log_source_set_options(&self->super, &options->super, stats_level, stats_source, stats_id, stats_instance, (options->flags & JR_THREADED), TRUE);

  log_pipe_unref(self->control);
  log_pipe_ref(control);
  self->control = control;
  self->options = options;
}

static void
__init_watches(JournalReader *self)
{
  IV_EVENT_INIT(&self->schedule_wakeup);
  self->schedule_wakeup.cookie = self;
  self->schedule_wakeup.handler = __wakeup_triggered;
  iv_event_register(&self->schedule_wakeup);

  IV_TASK_INIT(&self->restart_task);
  self->restart_task.cookie = self;
  self->restart_task.handler = __io_process_input;

  main_loop_io_worker_job_init(&self->io_job);
  self->io_job.user_data = self;
  self->io_job.work = (void (*)(void *)) __work_perform;
  self->io_job.completion = (void (*)(void *)) __work_finished;
}

JournalReader *
journal_reader_new(Journald *journal)
{
  JournalReader *self = g_new0(JournalReader, 1);
  log_source_init_instance(&self->super);
  self->super.wakeup = __reader_wakeup;
  self->super.super.init = __init;
  self->super.super.deinit = __deinit;
  self->super.super.free_fn = __free;
  self->persist_name = g_strdup("systemd-journal");
  self->journal = journal;
  __init_watches(self);
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

  options->initialized = TRUE;
}

void
journal_reader_options_defaults(JournalReaderOptions *options)
{
  log_source_options_defaults(&options->super);
  options->fetch_limit = DEFAULT_FETCH_LIMIT;
  options->default_pri = DEFAULT_PRIO;
  options->max_field_size = DEFAULT_FIELD_SIZE;
  options->super.read_old_records = FALSE;
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
