/*
 * Copyright (c) 2023 Hofi <hofione@gmail.com>
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

#import "darwinosl-source.h"
#import "darwinosl-source-oslog.h"
#include "darwinosl-source-persist.h"
#include "logthrsource/logthrfetcherdrv.h"
#include "ack-tracker/ack_tracker_factory.h"
#include "timeutils/misc.h"

/* TODO: Move these to darwinosl-source.h or a separate class once options and its handling need to be exposed */
void darwinosl_sd_options_defaults(DarwinOSLogSourceOptions *self,
                                   LogThreadedSourceWorkerOptions *super_source_options);
void darwinosl_sd_options_destroy(DarwinOSLogSourceOptions *self);
gboolean darwinosl_sd_options_get_read_old_records(DarwinOSLogSourceOptions *self);

struct _DarwinOSLogSourceOptions
{
  LogThreadedSourceWorkerOptions *super_source_options;
  MsgFormatOptions *format_options;

  gchar *filter_predicate;
  gboolean go_reverse;
  gboolean do_not_use_bookmark;
  guint max_bookmark_distance;
  guint fetch_delay;
  guint fetch_limit;
  gint fetch_retry_delay;
};

typedef struct _DarwinOSLogSourceDriver
{
  LogThreadedSourceDriver super;

  DarwinOSLogSourceOptions options;
  DarwinOSLogSourcePersist *log_source_persist;
  DarwinOSLogSourcePosition log_source_position;

  GAtomicCounter exit_requested;
  guint curr_fetch_in_run;
  const gchar *persist_name;
  const gchar *stat_persist_name;

  /* NOTE: Naming convention difference is intentional here, camelCase is ObjC default.
           and signs nicely those variables need different kind of care because of ARC */
  __strong OSLogSource *osLogSource;
} DarwinOSLogSourceDriver;


static void
_check_restored_postion(DarwinOSLogSourceDriver *self, OSLogEntry *nextLogEntry)
{
  NSString *msgString = [self->osLogSource stringFromDarwinOSLogEntry:nextLogEntry];

  if (g_str_hash(msgString.UTF8String) == self->log_source_position.last_msg_hash)
    {
      msg_debug("darwinosl: Bookmark is restored fine",
                evt_tag_str("bookmark", [OSLogSource.RFC3339DateFormatter stringFromDateWithMicroseconds:
                                                                          nextLogEntry.date].UTF8String));
    }
  else
    {
      if (self->log_source_position.last_msg_hash)
        msg_info("darwinosl: Could not restore last bookmark (filter might be changed or max_bookmark_distance took effect?)",
                 evt_tag_str("bookmark", [OSLogSource.RFC3339DateFormatter stringFromDateWithMicroseconds:
                                                                           [NSDate dateWithTimeIntervalSince1970:self->log_source_position.log_position]].UTF8String),
                 evt_tag_str("new start position", [OSLogSource.RFC3339DateFormatter stringFromDateWithMicroseconds:
                                                                                     nextLogEntry.date].UTF8String));
      else
        msg_debug("darwinosl: No last msg hash found",
                  evt_tag_str("new start position", [OSLogSource.RFC3339DateFormatter stringFromDateWithMicroseconds:
                                                                                      nextLogEntry.date].UTF8String));
    }
}

static gboolean
_log_position_date_from_persist(DarwinOSLogSourceDriver *self, NSDate **startDate)
{
  darwinosl_source_persist_load_position(self->log_source_persist, &self->log_source_position);

  /* NOTE: Here could be some internal logic to solve clashes might be caused by bookmarks saved with different bookmarking options, but
           based on read-old-records() we just let the user manually solve this (e.g. clear the persis-file if needed)
  */
  NSDate *maxBookmarkDistanceDate = [NSDate dateWithTimeIntervalSinceNow:-1 * (NSTimeInterval)(
                                              self->options.max_bookmark_distance)];
  if (self->options.max_bookmark_distance > 0)
    *startDate = maxBookmarkDistanceDate;

  gboolean should_start_from_now = (FALSE == darwinosl_sd_options_get_read_old_records(&self->options) &&
                                    (self->log_source_position.log_position == 0.0 || self->options.do_not_use_bookmark));
  if ((FALSE == self->options.do_not_use_bookmark && self->log_source_position.log_position != 0.0)
      || should_start_from_now)
    {
      NSDate *savedDate = [NSDate dateWithTimeIntervalSince1970:self->log_source_position.log_position];
      *startDate = (should_start_from_now ?
                    [NSDate dateWithTimeIntervalSinceNow:-1] :
                    (self->options.max_bookmark_distance > 0 ? [maxBookmarkDistanceDate laterDate:savedDate] : savedDate)
                   );
      return (FALSE == should_start_from_now);
    }
  return FALSE;
}

static LogMessage *
_log_message_from_string(const char *msg_cstring, MsgFormatOptions *format_options)
{
  int msg_len = strlen(msg_cstring);
  LogMessage *msg = msg_format_construct_message(format_options, (const guchar *) msg_cstring, msg_len);
  msg_format_parse_into(format_options, msg, (const guchar *) msg_cstring, msg_len);
  return msg;
}

static gboolean
_open_osl(LogThreadedSourceDriver *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;

  self->curr_fetch_in_run = 0;

  if ([self->osLogSource openStore] == nil)
    return FALSE;

  NSDate *startDate = nil;
  bool positionRestored = _log_position_date_from_persist(self, &startDate);
  msg_trace("darwinosl: Should start from",
            evt_tag_str("position",
                        (startDate ?
                         [OSLogSource.RFC3339DateFormatter stringFromDateWithMicroseconds:startDate].UTF8String :
                         "no saved position")));

  if ([self->osLogSource openEnumeratorWithDate:startDate
                         filterString:(self->options.filter_predicate ? [NSString stringWithUTF8String:self->options.filter_predicate] : nil)
                         options:(self->options.go_reverse ? OSLogEnumeratorReverse : 0)] == nil)
    {
      [self->osLogSource closeAll];
      return FALSE;
    }

  if (positionRestored)
    {
      OSLogEntry *nextLogEntry = [self->osLogSource fetchNextEntry];
      if (nextLogEntry)
        _check_restored_postion(self, nextLogEntry);
      else
        msg_warning("darwinosl: Could not fetch log entry after a successful position restore");
    }

  self->log_source_position.last_used_filter_predicate_hash = (self->options.filter_predicate ?
                                                              g_str_hash(self->options.filter_predicate) :
                                                              0);
  return TRUE;
}

static void
_close_osl(LogThreadedSourceDriver *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  [self->osLogSource closeAll];
}

/* runs in a dedicated thread */
static LogThreadedFetchResult
_fetch(LogThreadedSourceDriver *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;

  gboolean fetchedEnough = (self->options.fetch_limit && self->curr_fetch_in_run > self->options.fetch_limit);
  OSLogEntry *nextLogEntry = (fetchedEnough ? nil :[self->osLogSource fetchNextEntry]);

  if (nextLogEntry == nil)
    {
      msg_trace(nextLogEntry ? "darwinosl: Fetch limit reached" : "darwinosl: No more log data currently");
      msg_debug("darwinosl: Fetched logs in this run", evt_tag_long("fetch count", (long) self->curr_fetch_in_run));
      LogThreadedFetchResult result = {THREADED_FETCH_NO_DATA, NULL};
      return result;
    }

  self->curr_fetch_in_run++;

  const char *log_string = [self->osLogSource stringFromDarwinOSLogEntry:nextLogEntry].UTF8String;
  msg_trace("darwinosl: Msg string is composed", evt_tag_str("msg", log_string));

  /* NOTE: No other options unfortunately (there is no "current position" like getter)
            If there are multiple occurrences of the same time, the method returns the earliest occurrence.
            So, in a worst case there might be log duplications on restart from bookmark
  */
  self->log_source_position.log_position = [nextLogEntry.date timeIntervalSince1970];
  self->log_source_position.last_msg_hash = g_str_hash(log_string);

  LogSource *worker = (LogSource *) self->super.worker;
  Bookmark *bookmark = ack_tracker_request_bookmark(worker->ack_tracker);
  darwinosl_source_persist_fill_bookmark(self->log_source_persist, bookmark, self->log_source_position);
  msg_trace("darwinosl: Bookmark created",
            evt_tag_printf("position", "%.6f", self->log_source_position.log_position),
            evt_tag_printf("last message hash", "%u", self->log_source_position.last_msg_hash),
            evt_tag_printf("filter predicate hash", "%u", self->log_source_position.last_used_filter_predicate_hash));

  LogMessage *msg = _log_message_from_string(log_string, self->options.format_options);
  LogThreadedFetchResult result = {THREADED_FETCH_SUCCESS, msg};
  return result;
}

static void
_send(LogThreadedSourceDriver *s, LogMessage *msg)
{
  log_threaded_source_blocking_post(s, msg);
}

static void
_request_exit(LogThreadedSourceDriver *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *) s;
  g_atomic_counter_set(&self->exit_requested, TRUE);
}

/* TODO: Using some iv_timer or similar instead ?! */
static void
_sleep(DarwinOSLogSourceDriver *self, gdouble wait_time)
{
  const useconds_t microseconds_in_second = 1000 * 1000;
  const useconds_t min_sleep_time = 1;
  const useconds_t def_sleep_time = 1000;
  const useconds_t sleep_time = MAX(min_sleep_time, (useconds_t) MIN(def_sleep_time, wait_time * microseconds_in_second));
  GTimeVal now, last_check;

  g_get_current_time(&now);
  last_check = now;

  while (false == g_atomic_counter_get(&self->exit_requested))
    {
      usleep(sleep_time);

      gdouble diff = g_time_val_diff(&last_check, &now) / (gdouble) microseconds_in_second;
      if (diff >= wait_time)
        break;
      g_get_current_time(&last_check);
    }
}

/* runs in a dedicated thread */
static void
_run(LogThreadedSourceDriver *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *) s;

  while (false == g_atomic_counter_get(&self->exit_requested))
    {
      @autoreleasepool
      {
        if (FALSE == _open_osl(s))
          break;

        while (false == g_atomic_counter_get(&self->exit_requested))
          {
            @autoreleasepool
            {
              LogThreadedFetchResult res = _fetch(s);
              if (res.result == THREADED_FETCH_SUCCESS)
                _send(s, res.msg);
              else
                break;
            }
            _sleep(self, 1.0 / self->options.fetch_delay);
          }

        _close_osl(s);
      }
      if (self->options.fetch_retry_delay)
        _sleep(self, self->options.fetch_retry_delay);
    }
}

static const gchar *
_generate_persist_name(DarwinOSLogSourceDriver *self, const char *base_name, const char *name)
{
  gchar persist_name[1024];
  guint nameHash = 0;

  if (name == NULL && self->options.filter_predicate && strlen(self->options.filter_predicate))
    nameHash = g_str_hash(self->options.filter_predicate);

  const char *separator = (name || nameHash ? "." : "");
  if (nameHash)
    g_snprintf(persist_name, sizeof(persist_name), "%s%s%u", base_name, separator, nameHash);
  else
    g_snprintf(persist_name, sizeof(persist_name), "%s%s%s", base_name, separator, name ? : "");

  return g_strdup(persist_name);
}

static const gchar *
_get_persist_name(const LogPipe *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;

  if (self->persist_name == NULL)
    self->persist_name = _generate_persist_name(self, "darwinosl-source", s->persist_name);

  return self->persist_name;
}

static const gchar *
_format_stats_instance(LogThreadedSourceDriver *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;

  if (self->stat_persist_name == NULL)
    self->stat_persist_name = _generate_persist_name(self, "darwinosl", s->super.super.super.persist_name);

  return self->stat_persist_name;
}

static gboolean
_init(LogPipe *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config((LogPipe *)self);

  darwinosl_source_persist_init(self->log_source_persist, cfg->state, _get_persist_name(s));

  return log_threaded_source_driver_init_method(s);
}

static void
_free(LogPipe *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;

  /* NOTE: ARC takes care of the rest */
  self->osLogSource = nil;

  darwinosl_sd_options_destroy(&self->options);
  darwinosl_source_persist_free(self->log_source_persist);

  if (self->persist_name)
    g_free((gpointer)self->persist_name);
  if (self->stat_persist_name)
    g_free((gpointer)self->stat_persist_name);

  log_threaded_source_driver_free_method(s);
}

LogDriver *
darwinosl_sd_new(GlobalConfig *cfg)
{
  DarwinOSLogSourceDriver *self = g_new0(DarwinOSLogSourceDriver, 1);
  log_threaded_source_driver_init_instance(&self->super, cfg);

  g_atomic_counter_set(&self->exit_requested, FALSE);
  self->super.run = _run;
  self->super.request_exit = _request_exit;

  self->super.super.super.super.init = _init;
  self->super.super.super.super.free_fn = _free;
  self->super.super.super.super.generate_persist_name = _get_persist_name;

  self->super.format_stats_instance = _format_stats_instance;

  self->super.worker_options.ack_tracker_factory = consecutive_ack_tracker_factory_new();
  self->log_source_persist = darwinosl_source_persist_new();

  self->osLogSource = [OSLogSource new];
  darwinosl_sd_options_defaults(&self->options, &self->super.worker_options);

  return &self->super.super.super;
}

gboolean
darwinosl_sd_set_filter_predicate(LogDriver *s, const gchar *filter_predicate_str)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  if (self->options.filter_predicate)
    free (self->options.filter_predicate);
  self->options.filter_predicate = g_strdup(filter_predicate_str);
  return TRUE;
}

void
darwinosl_sd_set_go_reverse(LogDriver *s, gboolean new_value)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  self->options.go_reverse = new_value;
}

void
darwinosl_sd_set_do_not_use_bookmark(LogDriver *s, gboolean new_value)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  self->options.do_not_use_bookmark = new_value;
}

void
darwinosl_sd_set_max_bookmark_distance(LogDriver *s, guint new_value)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  self->options.max_bookmark_distance = new_value;
}

void
darwinosl_sd_set_log_fetch_delay(LogDriver *s, guint new_value)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  self->options.fetch_delay = new_value;
}

void
darwinosl_sd_set_log_fetch_retry_delay(LogDriver *s, guint new_value)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  self->options.fetch_retry_delay = new_value;
}

void
darwinosl_sd_set_log_fetch_limit(LogDriver *s, guint new_value)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  self->options.fetch_limit = new_value;
}

gboolean
darwinosl_sd_options_get_read_old_records(DarwinOSLogSourceOptions *self)
{
  return self->super_source_options->super.read_old_records;
}

void
darwinosl_sd_options_defaults(DarwinOSLogSourceOptions *self,
                              LogThreadedSourceWorkerOptions *super_source_options)
{
  self->super_source_options = super_source_options;

  /* No additional format options now, so intentionally referencing only, and not using msg_format_options_copy */
  self->format_options = &self->super_source_options->parse_options;

  self->super_source_options->super.stats_level = STATS_LEVEL0;
  self->super_source_options->super.stats_source = stats_register_type("darwinosl");

  self->super_source_options->super.read_old_records = TRUE;
  self->filter_predicate = NULL;
  self->go_reverse = FALSE;
  self->do_not_use_bookmark = FALSE;
  self->max_bookmark_distance = 0; /* 0 means no limit */
  self->fetch_delay = 10000;
  self->fetch_limit = 0; /* 0 means no limit */
  self->fetch_retry_delay = 1; /* seconds */
}

void
darwinosl_sd_options_destroy(DarwinOSLogSourceOptions *self)
{
  if (self->filter_predicate)
    {
      free (self->filter_predicate);
      self->filter_predicate = NULL;
    }

  /* Just referenced, not copied */
  self->format_options = NULL;
  self->super_source_options = NULL;
}
