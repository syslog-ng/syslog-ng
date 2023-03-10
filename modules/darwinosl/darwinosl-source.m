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

#include "darwinosl-source.h"
#include "darwinosl-source-persist.h"
#include "logthrsource/logthrsourcedrv.h"
#include "logthrsource/logthrfetcherdrv.h"
#include "ack-tracker/ack_tracker_factory.h"

#include "logmsg/logmsg.h"
#include "messages.h"

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <OSLog/OSLog.h>

typedef struct _DarwinOSLogSourceDriver
{
  LogThreadedFetcherDriver super;
  void (*original_worker_run)(LogThreadedSourceDriver *self);
  MsgFormatOptions *format_options;
  /* Custom darwinosl source options */
  gchar *filter_predicate_option;
  gboolean reset_bookmark_on_filter_predicate_change_option;
  gboolean start_from_now_option;
  gboolean always_start_from_now_option;
  gboolean do_not_use_bookmark_option;
  gboolean go_reverse_option;
  gint log_fetch_reopen_delay_option;

  DarwinOSLogSourcePersist *log_source_persist;
  DarwinOSLogSourcePosition log_source_position;

  /* NOTE: Naming convention difference is intentional here, camelCase is ObjC default.
           and signs nicely those variables need different kind of care because of ARC
  */
  __strong OSLogStore *logStore;
  __strong OSLogPosition *logPosition;
  __strong OSLogEnumerator *enumerator;
  __strong NSPredicate *filterPredicate;
  OSLogEnumeratorOptions enumeratorOptions;

  __strong NSDateFormatter *RFC3339DateFormatter;
} DarwinOSLogSourceDriver;


/* https://stackoverflow.com/questions/23684727/nsdateformatter-milliseconds-bug
*/
static NSString *customBugFixDateFormat = @"yyyy-MM-dd'T'HH:mm:ss'.%06.0f'ZZZZZ";

@interface NSDateFormatter (BugFix)
- (NSString *)stringFromDateWithMicroseconds:(NSDate *)theDate;
@end

@implementation NSDateFormatter (BugFix)
- (NSString *)stringFromDateWithMicroseconds:(NSDate *)theDate
{
  NSDateComponents *components = [self.calendar components:NSCalendarUnitNanosecond
                                                fromDate:theDate];
  double microseconds = lrint((double)components.nanosecond / (double)1000);
  NSString *dateTimeString = [self stringFromDate:theDate];
  NSString *formattedString = [NSString stringWithFormat:dateTimeString,
                                        microseconds];
  return formattedString;
}
@end

static int
_darwinosl_loglevel_to_pri(OSLogEntryLogLevel level)
{
  struct
  {
    const OSLogEntryLogLevel level;
    const int pri;
  } levels [] =
  {
    { OSLogEntryLogLevelUndefined, LOG_NOTICE },
    { OSLogEntryLogLevelDebug,     LOG_DEBUG },
    { OSLogEntryLogLevelInfo,      LOG_INFO },
    { OSLogEntryLogLevelNotice,    LOG_NOTICE },
    { OSLogEntryLogLevelError,     LOG_ERR },
    { OSLogEntryLogLevelFault,     LOG_CRIT },
  };
  const int maxLevels = sizeof(levels) / sizeof(levels[0]);
  return (level >= 0 && level < maxLevels ? levels[level].pri : LOG_NOTICE);
}

static NSString *
_log_string_from_darwinosl_log(DarwinOSLogSourceDriver *self, OSLogEntry *nextLogEntry)
{
  /* TODO: Consolidate this based on the real log type, handle all types correctly and fully */
  OSLogEntry<OSLogEntryFromProcess> *logEntry = (OSLogEntry<OSLogEntryFromProcess> *) nextLogEntry;
  BOOL hasProcessData = [logEntry respondsToSelector:@selector(process)];
  BOOL hasLevel = [logEntry respondsToSelector:@selector(level)];
  NSString *appName = hasProcessData ? [[logEntry process] stringByReplacingOccurrencesOfString:@" "
                                                           withString:@"\\0x20"] : @"-";
  NSString *procIDStr = hasProcessData ? [NSNumber numberWithUnsignedInt:[logEntry processIdentifier]].stringValue : @"-";
  NSString *timeStampStr = [self->RFC3339DateFormatter stringFromDateWithMicroseconds:logEntry.date];
  os_activity_id_t activity = hasProcessData ? [logEntry activityIdentifier] : 0;
  int priority = (NSInteger) (hasLevel ? _darwinosl_loglevel_to_pri([(OSLogEntryLog *) logEntry level]) : 0);

  /* By default using the rfc5424 Syslog Protocol
  SYSLOG-MSG = <PRI>VERSION SP -|TIMESTAMP SP -|HOSTNAME SP -|APP-NAME SP -|PROCID SP -|MSGID SP -|STRUCTURED-DATA [SP MSG] */
  NSString *formattedMsg = [NSString stringWithFormat:@"<%d>1 %@ - %@ %@ - - %lu %@",
                                     priority,      /* PRI */
                                     /* VERSION is hardcoded already */
                                     timeStampStr,  /* TIMESTAMP */
                                     /* HOSTNAME is skipped */
                                     appName,       /* APP-NAME */
                                     procIDStr,     /* PROCID */
                                     /* MSGID is skipped
                                        STRUCTURED-DATA is skipped
                                        Optional MSG part is from here
                                        TODO: activity and other additional properties might go into STRUCTURED-DATA */
                                     (unsigned long) activity,
                                     logEntry.composedMessage];
  return formattedMsg;
}

static LogMessage *
_log_message_with_string(NSString *msgString, MsgFormatOptions *format_options)
{
  const char *msg_cstring = msgString.UTF8String;
  int msg_len = strlen(msg_cstring);
  LogMessage *msg = msg_format_construct_message(format_options, (const guchar *) msg_cstring, msg_len);
  msg_format_parse_into(format_options, msg, (const guchar *) msg_cstring, msg_len);
  return msg;
}

static OSLogStore *
_open_osl_log_store(DarwinOSLogSourceDriver *self)
{
  NSError *error = nil;
  OSLogStore *logStore = nil;
  if (@available(macOS 12.0, *))
    logStore = [OSLogStore storeWithScope:OSLogStoreSystem error:&error];
  else
    logStore = [OSLogStore localStoreAndReturnError:&error];

  if (logStore == nil)
    msg_error("darwinosl: Error getting handle to OSLog store",
              evt_tag_long("error code", error.code),
              evt_tag_str("error", error.localizedDescription.UTF8String));
  return logStore;
}

static OSLogEntry *
_fetch_next_log_entry(DarwinOSLogSourceDriver *self)
{
  OSLogEntry *nextLogEntry;

  /* Skip now the Activity and the Boundary ones */
  while ((nextLogEntry = [self->enumerator nextObject]) &&
         ([nextLogEntry isKindOfClass:OSLogEntryActivity.class] || [nextLogEntry isKindOfClass:OSLogEntryBoundary.class]))
    continue;

  return nextLogEntry;
}

static OSLogEnumerator *
_open_osl_log_enumerator(DarwinOSLogSourceDriver *self)
{
  NSError *error = nil;
  OSLogEnumerator *enumerator = [self->logStore entriesEnumeratorWithOptions:self->enumeratorOptions
                                                position:self->logPosition
                                                predicate:self->filterPredicate
                                                error:&error];
  if (enumerator == nil)
    msg_error("darwinosl: Could not get OSLog enumerator",
              evt_tag_long("error code", error.code),
              evt_tag_str("error", error.localizedDescription.UTF8String));
  return enumerator;
}

static void
_check_restored_postion(DarwinOSLogSourceDriver *self, OSLogEntry *nextLogEntry)
{
  NSString *msgString = _log_string_from_darwinosl_log(self, nextLogEntry);

  if (g_str_hash(msgString.UTF8String) == self->log_source_position.last_msg_hash)
    {
      msg_debug("darwinosl: Bookmark is restored fine",
                evt_tag_str("bookmark", [self->RFC3339DateFormatter stringFromDateWithMicroseconds:nextLogEntry.date].UTF8String));
    }
  else
    {
      if (self->log_source_position.last_msg_hash)
        msg_info("darwinosl: Could not restore last bookmark (filter might be changed?)",
                 evt_tag_str("bookmark", [self->RFC3339DateFormatter stringFromDateWithMicroseconds:
                                                                     [NSDate dateWithTimeIntervalSince1970:self->log_source_position.log_position]].UTF8String),
                 evt_tag_str("new start position", [self->RFC3339DateFormatter stringFromDateWithMicroseconds:
                                                                               nextLogEntry.date].UTF8String));
      else
        msg_debug("darwinosl: No last msg hash found",
                  evt_tag_str("new start position", [self->RFC3339DateFormatter stringFromDateWithMicroseconds:
                                                                                nextLogEntry.date].UTF8String));
    }
}

static gboolean
_should_reset_postion_on_option_changes(DarwinOSLogSourceDriver *self)
{
  gboolean should_reset_pos = FALSE;
  guint curr_predicate_hash = (self->filter_predicate_option && strlen(self->filter_predicate_option) ?
                               g_str_hash(self->filter_predicate_option) :
                               0);
  gboolean predicate_option_changed = curr_predicate_hash != self->log_source_position.last_used_filter_predicate_hash;

  if (predicate_option_changed)
    {
      msg_info("darwinosl: Filter predicate has changed", evt_tag_str("new predicate", self->filter_predicate_option));
      if (self->reset_bookmark_on_filter_predicate_change_option)
        {
          should_reset_pos = TRUE;
          msg_debug("darwinosl: Bookmark position reset requested on filter predicate change, resetting");
        }
      else
        msg_debug("darwinosl: Bookmark position reset is NOT requested on filter predicate change, continuing from last log position");
    }

  /* TODO: After decided how the options will be stored add persist storage of the start_from_now_option and comparision to old value */
  gboolean start_from_now_option_changed = FALSE;
  if (start_from_now_option_changed)
    {
      should_reset_pos = TRUE;
      msg_info("darwinosl: Option start_from_now has changed", evt_tag_int("new option", (int) self->start_from_now_option));
      msg_debug("darwinosl: Bookmark position is resetting on start_from_now option change");
    }

  return should_reset_pos;
}

static gboolean
_log_position_from_persist(DarwinOSLogSourceDriver *self)
{
  darwinosl_source_persist_load_position(self->log_source_persist, &self->log_source_position);

  if (_should_reset_postion_on_option_changes(self))
    {
      self->log_source_position.log_position = 0;
      self->log_source_position.last_msg_hash = 0;
    }

  gboolean should_start_from_now = (self->log_source_position.log_position == 0.0 && self->start_from_now_option) ||
                                   self->always_start_from_now_option;
  if ((FALSE == self->do_not_use_bookmark_option && self->log_source_position.log_position != 0.0)
      || should_start_from_now)
    {
      NSDate *startDate = (should_start_from_now ?
                           NSDate.now :
                           [NSDate dateWithTimeIntervalSince1970:self->log_source_position.log_position]);
      self->logPosition = [self->logStore positionWithDate:startDate];

      return (self->logPosition && FALSE == should_start_from_now);
    }
  return FALSE;
}

static gboolean
_open_osl(LogThreadedFetcherDriver *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;

  g_assert(self->logStore == nil);
  self->logStore = _open_osl_log_store(self);
  if (self->logStore == nil)
    return FALSE;

  bool positionRestored = _log_position_from_persist(self);

  g_assert(self->enumerator == nil);
  self->enumerator = _open_osl_log_enumerator(self);
  if (self->enumerator == nil)
    return FALSE;

  if (positionRestored)
    {
      OSLogEntry *nextLogEntry = _fetch_next_log_entry(self);
      if (nextLogEntry)
        _check_restored_postion(self, nextLogEntry);
      else
        msg_error("darwinosl: Could not fetch log entry after a successful position restore");
    }

  self->log_source_position.last_used_filter_predicate_hash = (self->filterPredicate ?
                                                              g_str_hash(self->filterPredicate.predicateFormat.UTF8String) :
                                                              0);
  return TRUE;
}

static void
_close_osl(LogThreadedFetcherDriver *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;

  self->enumerator = nil;
  self->logStore = nil;
  self->logPosition = nil;
}

static void
_worker_run_with_autorelease_pool(LogThreadedSourceDriver *s)
{
  @autoreleasepool
  {
    DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
    self->original_worker_run(s);
  }
}

/* runs in a dedicated thread */
static LogThreadedFetchResult
_fetch(LogThreadedFetcherDriver *s)
{
  @autoreleasepool
  {
    DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
    OSLogEntry *nextLogEntry = _fetch_next_log_entry(self);

    if (nextLogEntry == nil)
      {
        /* Intentionally using close/reopen instead of THREADED_FETCH_NO_DATA and retry,
           as no such mechanism in case of the OSLog, seems the enumerator and the acquired log store handle
           belong to some kind of snapshot, so if the enumerator reached the end of the current snapshot
           both the handler and the enumerator must be re-acquired that is closer to the open/close behaviour
         */
        _close_osl(s);
        LogThreadedFetchResult result = {THREADED_FETCH_NOT_CONNECTED, NULL};
        return result;
      }

    NSString *logString = _log_string_from_darwinosl_log(self, nextLogEntry);
    msg_trace("darwinosl: Msg string is composed", evt_tag_str("msg", logString.UTF8String));

    /* NOTE: No other options unfortunately (there is no "current position" like getter)
             If there are multiple occurrences of the same time, the method returns the earliest occurrence.
             So, in a worst case there might be log duplications on restart from bookmark
     */
    self->log_source_position.log_position = [nextLogEntry.date timeIntervalSince1970];
    self->log_source_position.last_msg_hash = g_str_hash(logString.UTF8String);

    LogSource *worker = (LogSource *) self->super.super.worker;
    Bookmark *bookmark = ack_tracker_request_bookmark(worker->ack_tracker);
    darwinosl_source_persist_fill_bookmark(self->log_source_persist, bookmark, self->log_source_position);
    msg_trace("darwinosl: Bookmark created",
              evt_tag_printf("position", "%.6f", self->log_source_position.log_position),
              evt_tag_printf("last message hash", "%u", self->log_source_position.last_msg_hash),
              evt_tag_printf("filter predicate hash", "%u", self->log_source_position.last_used_filter_predicate_hash));

    LogMessage *msg = _log_message_with_string(logString, self->format_options);
    LogThreadedFetchResult result = {THREADED_FETCH_SUCCESS, msg};
    return result;
  }
}

static NSDateFormatter *
_get_default_date_formatter(void)
{
  NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
  formatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
  formatter.dateFormat = customBugFixDateFormat;
  formatter.timeZone = [NSTimeZone localTimeZone];
  return formatter;
}

static const gchar *
_get_persist_name(const LogPipe *s)
{
  static gchar persist_name[1024];

  if (persist_name[0] == 0)
    {
      if (s->persist_name)
        g_snprintf(persist_name, sizeof(persist_name), "darwinosl.%s", s->persist_name);
      else
        {
          /* TODO: Need something unique here for persist name composition */
          g_snprintf(persist_name, sizeof(persist_name), "darwinosl,default");
        }
    }

  return persist_name;
}

static const gchar *
_format_stats_instance(LogThreadedSourceDriver *s)
{
  static gchar persist_name[1024];

  if (persist_name[0] == 0)
    {
      if (s->super.super.super.persist_name)
        g_snprintf(persist_name, sizeof(persist_name), "darwinosl-source,%s", s->super.super.super.persist_name);
      else
        {
          /* TODO: Need something unique here for persist name composition */
          g_snprintf(persist_name, sizeof(persist_name), "darwinosl-source,default");
        }
    }
  return persist_name;
}

static gboolean
_init(LogPipe *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config((LogPipe *)self);

  /* no additional format options now, so intentionally not using msg_format_options_copy */
  self->format_options = &self->super.super.worker_options.parse_options;

  self->super.time_reopen = self->log_fetch_reopen_delay_option;

  self->RFC3339DateFormatter = _get_default_date_formatter();
  self->filterPredicate = (self->filter_predicate_option && strlen(self->filter_predicate_option)> 0 ?
                           [NSPredicate predicateWithFormat:[NSString stringWithUTF8String:self->filter_predicate_option]] :
                           nil);
  self->enumeratorOptions = (self->go_reverse_option ? OSLogEnumeratorReverse : 0);

  darwinosl_source_persist_init(self->log_source_persist, cfg->state, _get_persist_name(s));

  self->original_worker_run = self->super.super.run;
  self->super.super.run = _worker_run_with_autorelease_pool;

  return log_threaded_fetcher_driver_init_method(s);
}

static void
_free(LogPipe *s)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;

  /* NOTE: ARC takes care of these, but to stay consistent and keep it visible */
  self->logStore = nil;
  self->enumerator = nil;
  self->logPosition = nil;
  self->filterPredicate = nil;
  self->RFC3339DateFormatter = nil;

  free (self->filter_predicate_option);
  darwinosl_source_persist_free(self->log_source_persist);
  log_threaded_fetcher_driver_free_method(s);
}

LogDriver *
darwinosl_sd_new(GlobalConfig *cfg)
{
  DarwinOSLogSourceDriver *self = g_new0(DarwinOSLogSourceDriver, 1);
  log_threaded_fetcher_driver_init_instance(&self->super, cfg);

  self->super.super.super.super.super.init = _init;
  self->super.super.super.super.super.free_fn = _free;
  self->super.super.super.super.super.generate_persist_name = _get_persist_name;

  self->super.connect = _open_osl;
  self->super.disconnect = _close_osl;
  self->super.fetch = _fetch;

  self->super.super.format_stats_instance = _format_stats_instance;
  self->super.super.worker_options.super.stats_level = STATS_LEVEL0;
  self->super.super.worker_options.super.stats_source = stats_register_type("darwinosl");

  self->super.super.worker_options.ack_tracker_factory = consecutive_ack_tracker_factory_new();
  self->log_source_persist = darwinosl_source_persist_new();

  /* Consider using an option class instead and/or usage of new class specific flags() ?! */
  self->reset_bookmark_on_filter_predicate_change_option = TRUE;
  self->start_from_now_option = FALSE;
  self->always_start_from_now_option = FALSE;
  self->do_not_use_bookmark_option = FALSE;
  self->go_reverse_option = FALSE;
  self->log_fetch_reopen_delay_option = 1;

  return &self->super.super.super.super;
}

/* For predicate hints see:  log help predicates */
gboolean
darwinosl_sd_set_filter_predicate(LogDriver *s, const gchar *filter_predicate_str)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  if (self->filter_predicate_option)
    free (self->filter_predicate_option);
  self->filter_predicate_option = g_strdup(filter_predicate_str);
  return TRUE;
}

void
darwinosl_sd_set_reset_postion_on_filter_change(LogDriver *s, gboolean new_value)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  self->reset_bookmark_on_filter_predicate_change_option = new_value;
}

void
darwinosl_sd_set_start_from_now(LogDriver *s, gboolean new_value)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  self->start_from_now_option = new_value;
}

void
darwinosl_sd_set_always_start_from_now(LogDriver *s, gboolean new_value)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  self->always_start_from_now_option = new_value;
}

void
darwinosl_sd_set_do_not_use_bookmark(LogDriver *s, gboolean new_value)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  self->do_not_use_bookmark_option = new_value;
}

void
darwinosl_sd_set_go_reverse(LogDriver *s, gboolean new_value)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  self->go_reverse_option = new_value;
}

void
darwinosl_sd_set_log_fetch_reopen_delay(LogDriver *s, guint new_value)
{
  DarwinOSLogSourceDriver *self = (DarwinOSLogSourceDriver *)s;
  self->log_fetch_reopen_delay_option = new_value;
}
