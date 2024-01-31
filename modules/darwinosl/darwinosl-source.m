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
#include "stats/stats-cluster-single.h"
#include "stats/aggregator/stats-aggregator-registry.h"
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

  StatsAggregator *max_message_size;
  StatsAggregator *average_messages_size;
  StatsAggregator *CPS;

  /* NOTE: Naming convention difference is intentional here, camelCase is ObjC default.
           and signs nicely those variables need different kind of care because of ARC */
  __strong OSLogSource *osLogSource;
} DarwinOSLogSourceDriver;

static void
_log_reader_insert_msg_length_stats(DarwinOSLogSourceDriver *self, gsize len)
{
  stats_aggregator_add_data_point(self->max_message_size, len);
  stats_aggregator_add_data_point(self->average_messages_size, len);
}

static void
_register_aggregated_stats(DarwinOSLogSourceDriver *self)
{
  LogThreadedSourceWorker *worker = self->super.workers[0];
  StatsClusterKeyBuilder *kb = worker->super.metrics.stats_kb;
  stats_cluster_key_builder_push(kb);
  {
    LogSourceOptions *super_options = &self->options.super_source_options->super;
    gchar *stats_id = worker->super.stats_id;
    gchar stats_instance[1024];
    const gchar *instance_name = stats_cluster_key_builder_format_legacy_stats_instance(kb, stats_instance,
                                 sizeof(stats_instance));
    stats_aggregator_lock();
    StatsClusterKey sc_key;

    // msg_size_max
    stats_cluster_single_key_legacy_set_with_name(&sc_key,
                                                  super_options->stats_source | SCS_SOURCE,
                                                  stats_id,
                                                  instance_name, "msg_size_max");
    stats_register_aggregator_maximum(super_options->stats_level, &sc_key,
                                      &self->max_message_size);

    // msg_size_avg
    stats_cluster_single_key_legacy_set_with_name(&sc_key,
                                                  super_options->stats_source | SCS_SOURCE,
                                                  stats_id,
                                                  instance_name, "msg_size_avg");
    stats_register_aggregator_average(super_options->stats_level, &sc_key,
                                      &self->average_messages_size);

    // eps
    stats_cluster_single_key_legacy_set_with_name(&sc_key,
                                                  super_options->stats_source | SCS_SOURCE,
                                                  stats_id,
                                                  instance_name, "eps");
    stats_register_aggregator_cps(super_options->stats_level, &sc_key,
                                  worker->super.metrics.recvd_messages_key,
                                  SC_TYPE_SINGLE_VALUE,
                                  &self->CPS);
    stats_aggregator_unlock();
  }
  stats_cluster_key_builder_pop(kb);
}

static void
_unregister_aggregated_stats(DarwinOSLogSourceDriver *self)
{
  stats_aggregator_lock();

  stats_unregister_aggregator(&self->max_message_size);
  stats_unregister_aggregator(&self->average_messages_size);
  stats_unregister_aggregator(&self->CPS);

  stats_aggregator_unlock();
}

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
                 evt_tag_str("new_start_position", [OSLogSource.RFC3339DateFormatter stringFromDateWithMicroseconds:
                                                    nextLogEntry.date].UTF8String));
      else
        msg_debug("darwinosl: No last msg hash found",
                  evt_tag_str("new_start_position", [OSLogSource.RFC3339DateFormatter stringFromDateWithMicroseconds:
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

static gsize
_log_message_from_string(const char *msg_cstring, MsgFormatOptions *format_options, LogMessage **out_msg)
{
  gsize msg_len = strlen(msg_cstring);
  *out_msg = msg_format_construct_message(format_options, (const guchar *) msg_cstring, msg_len);
  msg_format_parse_into(format_options, *out_msg, (const guchar *) msg_cstring, msg_len);
  return msg_len;
}

static gboolean
_open_osl(DarwinOSLogSourceDriver *self)
{
  self->curr_fetch_in_run = 0;

  if ([self->osLogSource openStore] == nil)
    return FALSE;

  NSDate *startDate = nil;
  bool positionRestored = _log_position_date_from_persist(self, &startDate);
  NSString *filterString = (self->options.filter_predicate ? [NSString stringWithUTF8String:
                                                              self->options.filter_predicate] : nil);
  OSLogEnumeratorOptions options = (self->options.go_reverse ? OSLogEnumeratorReverse : 0);

  msg_trace("darwinosl: Should start from",
            evt_tag_str("start_position",
                        (startDate ?
                         [OSLogSource.RFC3339DateFormatter stringFromDateWithMicroseconds:startDate].UTF8String :
                         "no saved position")));
  if ([self->osLogSource openEnumeratorWithDate:startDate filterString:filterString options:options] == nil)
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

static inline void
_close_osl(DarwinOSLogSourceDriver *self)
{
  [self->osLogSource closeAll];
}

/* runs in a dedicated thread */
static LogThreadedFetchResult
_fetch(DarwinOSLogSourceDriver *self)
{
  gboolean fetchedEnough = (self->options.fetch_limit && self->curr_fetch_in_run > self->options.fetch_limit);
  OSLogEntry *nextLogEntry = (fetchedEnough ? nil :[self->osLogSource fetchNextEntry]);

  if (nextLogEntry == nil)
    {
      msg_trace(nextLogEntry ? "darwinosl: Fetch limit reached" : "darwinosl: No more log data currently");
      msg_debug("darwinosl: Fetched logs in this run", evt_tag_long("fetch_count", (long) self->curr_fetch_in_run));
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

  LogSource *worker = (LogSource *) self->super.workers[0];
  Bookmark *bookmark = ack_tracker_request_bookmark(worker->ack_tracker);
  darwinosl_source_persist_fill_bookmark(self->log_source_persist, bookmark, self->log_source_position);
  msg_trace("darwinosl: Bookmark created",
            evt_tag_printf("position", "%.6f", self->log_source_position.log_position),
            evt_tag_printf("last message hash", "%u", self->log_source_position.last_msg_hash),
            evt_tag_printf("filter predicate hash", "%u", self->log_source_position.last_used_filter_predicate_hash));

  LogMessage *msg;
  gsize msg_len = _log_message_from_string(log_string, self->options.format_options, &msg);
  log_msg_set_value_to_string(msg, LM_V_TRANSPORT, "local+darwinoslog");
  LogThreadedFetchResult result = {THREADED_FETCH_SUCCESS, msg};
  _log_reader_insert_msg_length_stats(self, msg_len);
  return result;
}

static inline void
_send(LogThreadedSourceWorker *worker, LogMessage *msg)
{
  log_threaded_source_worker_blocking_post(worker, msg);
}

static void
_request_exit(LogThreadedSourceWorker *worker)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *) worker->control;
  g_atomic_counter_set(&self->exit_requested, TRUE);
}

static void
_sleep(DarwinOSLogSourceDriver *self, gdouble wait_time)
{
  const useconds_t min_sleep_time = 1;
  const useconds_t def_sleep_time = 1000;
  const useconds_t sleep_time = MAX(min_sleep_time, (useconds_t) MIN(def_sleep_time, wait_time * USEC_PER_SEC));
  struct timespec now, last_check;

  clock_gettime(CLOCK_MONOTONIC, &now);
  last_check = now;

  while (FALSE == g_atomic_counter_get(&self->exit_requested))
    {
      usleep(sleep_time);

      gdouble diff = timespec_diff_usec(&last_check, &now) / (gdouble) USEC_PER_SEC;

      if (diff >= wait_time)
        break;
      clock_gettime(CLOCK_MONOTONIC, &last_check);
    }
}

/* runs in a dedicated thread */
static void
_run(LogThreadedSourceWorker *worker)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *) worker->control;
  const gdouble iteration_sleep_time = 1.0 / self->options.fetch_delay;

  while (FALSE == g_atomic_counter_get(&self->exit_requested))
    {
      @autoreleasepool
      {
        if (FALSE == _open_osl(self))
          break;

        while (FALSE == g_atomic_counter_get(&self->exit_requested))
          {
            @autoreleasepool
            {
              LogThreadedFetchResult res = _fetch(self);
              if (res.result == THREADED_FETCH_SUCCESS)
                _send(worker, res.msg);
              else
                break;
            }
            _sleep(self, iteration_sleep_time);
          }

        _close_osl(self);
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

static void
_format_stats_key(LogThreadedSourceDriver *s, StatsClusterKeyBuilder *kb)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;

  if (self->stat_persist_name == NULL)
    self->stat_persist_name = _generate_persist_name(self, "darwinosl", s->super.super.super.persist_name);
}

static gboolean
_init(LogPipe *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config((LogPipe *)self);

  darwinosl_source_persist_init(self->log_source_persist, cfg->state, _get_persist_name(s));

  gboolean ret = log_threaded_source_driver_init_method(s);
  if (ret)
    _register_aggregated_stats(self);

  return ret;
}

static gboolean
_deinit(LogPipe *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;

  _unregister_aggregated_stats(self);

  return log_threaded_source_driver_deinit_method(s);
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

static LogThreadedSourceWorker *
_construct_worker(LogThreadedSourceDriver *s, gint worker_index)
{
  /* TODO: DarwinOSLogSourceDriver uses the multi-worker API, but it is not prepared now to work with more than one worker. */
  g_assert(s->num_workers == 1);

  LogThreadedSourceWorker *worker = g_new0(LogThreadedSourceWorker, 1);
  log_threaded_source_worker_init_instance(worker, s, worker_index);

  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *) s;
  g_atomic_counter_set(&self->exit_requested, FALSE);
  worker->run = _run;
  worker->request_exit = _request_exit;

  return worker;
}

LogDriver *
darwinosl_sd_new(GlobalConfig *cfg)
{
  DarwinOSLogSourceDriver *self = g_new0(DarwinOSLogSourceDriver, 1);
  log_threaded_source_driver_init_instance(&self->super, cfg);

  self->super.super.super.super.init = _init;
  self->super.super.super.super.deinit = _deinit;
  self->super.super.super.super.free_fn = _free;
  self->super.super.super.super.generate_persist_name = _get_persist_name;

  self->super.worker_options.ack_tracker_factory = consecutive_ack_tracker_factory_new();
  self->super.worker_construct = _construct_worker;

  self->super.format_stats_key = _format_stats_key;

  self->log_source_persist = darwinosl_source_persist_new();

  self->osLogSource = [OSLogSource new];

  darwinosl_sd_options_defaults(&self->options, &self->super.worker_options);

  return &self->super.super.super;
}

/* Options */

/*
 * FIXME: Seems, in a debug build, there is no chance to catch the NSInvalidArgumentException, so
 *        this will always crash if the predicte string has certain parsing issues.
 */
gboolean
_try_test_filter_predicate_str(const gchar *filter_predicate_str)
{
  gboolean succ = TRUE;
  if (filter_predicate_str)
    {
      NSString *filterString = [NSString stringWithUTF8String:filter_predicate_str];
      if (filterString.length > 0)
        {
          NSPredicate *filterPredicate = nil;
          succ = FALSE;
          @try
            {
              if ((filterPredicate = [NSPredicate predicateWithFormat:filterString]) != nil)
                succ = TRUE;
            }
          @catch(NSException *e)
            {
              msg_error("darwinosl: Error in filter predicate",
                        evt_tag_str("predicate", filter_predicate_str),
                        evt_tag_str("error", [NSString stringWithFormat:@"%@", e].UTF8String));
            }
        }
    }
  return succ;
}

gboolean
darwinosl_sd_set_filter_predicate(LogDriver *s, const gchar *filter_predicate_str)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;

  if (self->options.filter_predicate)
    free (self->options.filter_predicate);
  self->options.filter_predicate = g_strdup(filter_predicate_str);

  if (self->options.filter_predicate)
    return _try_test_filter_predicate_str(self->options.filter_predicate);
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
  self->super_source_options->super.stats_level = STATS_LEVEL0;
  self->super_source_options->super.stats_source = stats_register_type("darwinosl");
  self->super_source_options->super.read_old_records = FALSE;

  /* No additional format options now, so intentionally referencing only, and not using msg_format_options_copy */
  self->format_options = &self->super_source_options->parse_options;

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
