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

#include "darwinosl-source-oslog.h"
#include "messages.h"

/* https://stackoverflow.com/questions/23684727/nsdateformatter-milliseconds-bug */
static NSString *customBugFixDateFormat = @"yyyy-MM-dd'T'HH:mm:ss'.%06.0f'ZZZZZ";

@implementation NSDateFormatter (BugFix)

- (NSString *) stringFromDateWithMicroseconds:(NSDate *)theDate
{
  NSDateComponents *components = [self.calendar components:NSCalendarUnitNanosecond
                                                fromDate:theDate];
  double microseconds = lrint((double)components.nanosecond / (double)1000);
  NSString *dateTimeFormatString = [self stringFromDate:theDate];
  NSString *formattedString = [NSString stringWithFormat:dateTimeFormatString, microseconds];
  return formattedString;
}

@end

@interface OSLogSource ()

@property (strong, nonatomic) OSLogStore *logStore;
@property (strong, nonatomic) OSLogPosition *logPosition;
@property (strong, nonatomic) OSLogEnumerator *enumerator;
@property (assign, nonatomic) OSLogEnumeratorOptions enumeratorOptions;
@property (strong, nonatomic) NSPredicate *filterPredicate;

@end

@implementation OSLogSource

+ (NSDateFormatter *) defaultDateFormatter
{
  NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
  formatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
  formatter.dateFormat = customBugFixDateFormat;
  formatter.timeZone = [NSTimeZone localTimeZone];
  return formatter;
}

+ (NSDateFormatter *) RFC3339DateFormatter
{
  static NSDateFormatter *_RFC3339DateFormatter = nil;
  if (_RFC3339DateFormatter == nil)
    _RFC3339DateFormatter = OSLogSource.defaultDateFormatter;
  return _RFC3339DateFormatter;
}

- (OSLogStore *) openStore
{
  g_assert(self.logStore == nil);
  NSError *error = nil;

  if (@available(macOS 12.0, *))
    self.logStore = [OSLogStore storeWithScope:OSLogStoreSystem error:&error];
  else
    self.logStore = [OSLogStore localStoreAndReturnError:&error];

  if (self.logStore == nil)
    msg_error("darwinosl: Error getting handle to OSLog store",
              evt_tag_long("error_code", error.code),
              evt_tag_str("error", error.localizedDescription.UTF8String));

  return self.logStore;
}

- (OSLogEnumerator *) openEnumeratorWithDate:(NSDate *)startDate
  filterString:(NSString *)filterString
  options:(OSLogEnumeratorOptions)options
{
  g_assert(self.enumerator == nil);
  // NOTE: This still can rise an uncought exception regardless the try/catch block here,
  //       see darwinosl-source.m _try_test_filter_predicate_str for more info
  @try
    {
      NSPredicate *filterPredicate = (filterString.length > 0 ? [NSPredicate predicateWithFormat:filterString] : nil);
      NSError *error = nil;

      self.enumeratorOptions = options;
      self.filterPredicate = filterPredicate;
      self.logPosition = [self.logStore positionWithDate:startDate];

      self.enumerator = [self.logStore entriesEnumeratorWithOptions:self.enumeratorOptions
                                       position:self.logPosition
                                       predicate:self.filterPredicate
                                       error:&error];
      if (self.enumerator == nil)
        msg_error("darwinosl: Could not get OSLog enumerator",
                  evt_tag_long("error_code", error.code),
                  evt_tag_str("error", error.localizedDescription.UTF8String));

    }
  @catch(NSException *e)
    {
      msg_error("darwinosl: Could not open enumerator with filter predicate",
                evt_tag_str("predicate", filterString.length ? filterString.UTF8String : ""),
                evt_tag_str("error", [NSString stringWithFormat:@"%@", e].UTF8String));
    }

  return self.enumerator;
}

- (void) closeAll
{
  self.logPosition = nil;
  self.enumerator = nil;
  self.logStore = nil;
}

- (OSLogEntry *) fetchNextEntry
{
  OSLogEntry *nextLogEntry = [self.enumerator nextObject];
  return nextLogEntry;
}

+ (int) darwinOSLogLevelToPriority:(OSLogEntryLogLevel)level
{
  struct
  {
    const OSLogEntryLogLevel level;
    const int pri;
  } levels [] =
  {
    { OSLogEntryLogLevelUndefined, LOG_NOTICE },  // 5
    { OSLogEntryLogLevelDebug,     LOG_DEBUG },   // 7
    { OSLogEntryLogLevelInfo,      LOG_INFO },    // 6
    { OSLogEntryLogLevelNotice,    LOG_NOTICE },  // 5
    { OSLogEntryLogLevelError,     LOG_ERR },     // 3
    { OSLogEntryLogLevelFault,     LOG_CRIT },    // 2
  };
  const int maxLevels = sizeof(levels) / sizeof(levels[0]);
  return (level >= 0 && level < maxLevels ? levels[level].pri : LOG_NOTICE);
}


- (NSString *) stringFromDarwinOSLogEntry:(OSLogEntry *)nextLogEntry
{
  BOOL hasProcessData = [nextLogEntry conformsToProtocol:@protocol(OSLogEntryFromProcess)];
  BOOL hasPayLoad = [nextLogEntry conformsToProtocol:@protocol(OSLogEntryWithPayload)];
  BOOL hasLevel = [nextLogEntry respondsToSelector:@selector(level)];
  OSLogEntry<OSLogEntryFromProcess, OSLogEntryWithPayload> *logEntry =
    (OSLogEntry<OSLogEntryFromProcess, OSLogEntryWithPayload> *) nextLogEntry;
  NSString *appName = (hasProcessData ?
                       /* RFC5424 does not allow 0x20 in APP-NAME */
                       [[logEntry process] stringByReplacingOccurrencesOfString:@" "
                                           withString:@"\\0x20"] :
                       @"-");
  NSString *procIDStr = (hasProcessData ? [NSNumber numberWithUnsignedInt:[logEntry processIdentifier]].stringValue :
                         @"-");
  NSString *timeStampStr = [OSLogSource.RFC3339DateFormatter stringFromDateWithMicroseconds:logEntry.date];
  os_activity_id_t activity = (hasProcessData ? [logEntry activityIdentifier] : 0);
  int priority = (int) (hasLevel ? [OSLogSource darwinOSLogLevelToPriority:[(OSLogEntryLog *) logEntry level]] : 0);
  NSString *subsystem = (hasPayLoad && [logEntry subsystem] ?
                         [NSString stringWithFormat:@" (%@)", [logEntry subsystem]] :
                         @"");
  NSString *category = (hasPayLoad && [logEntry category] ?
                        [NSString stringWithFormat:@" [%@]", [logEntry category]] :
                        @"");

  /* By default using the RFC5424 Syslog Protocol
  SYSLOG-MSG = <PRI>VERSION SP -|TIMESTAMP SP -|HOSTNAME SP -|APP-NAME SP -|PROCID SP -|MSGID SP -|STRUCTURED-DATA [SP MSG] */
  NSString *formattedMsg = [NSString stringWithFormat:@"<%d>1 %@ - %@ %@ - - %lu%@%@ %@",
                                     priority,      /* PRI */
                                     /* VERSION is hardcoded already */
                                     timeStampStr,  /* TIMESTAMP */
                                     /* HOSTNAME is skipped */
                                     appName,       /* APP-NAME */
                                     procIDStr,     /* PROCID */
                                     /* MSGID is skipped */
                                     /* STRUCTURED-DATA is skipped */
                                     /* Optional MSG part is from here
                                        TODO: These additional properties might go into STRUCTURED-DATA */
                                     (unsigned long) activity,
                                     subsystem,
                                     category,
                                     logEntry.composedMessage];
  return formattedMsg;
}

@end
