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

#ifndef DARWINOS_SOURCE_OSLOG_H_INCLUDED
#define DARWINOS_SOURCE_OSLOG_H_INCLUDED

#include <OSLog/OSLog.h>

@interface NSDateFormatter (BugFix)

- (NSString *) stringFromDateWithMicroseconds:(NSDate *)theDate;

@end

@interface OSLogSource : NSObject

@property (class, readonly, nonatomic) NSDateFormatter *RFC3339DateFormatter;

- (OSLogStore *) openStore;
- (OSLogEnumerator *) openEnumeratorWithDate:(NSDate *)startDate
  filterString:(NSString *)filterString
  options:(OSLogEnumeratorOptions)options;
- (void) closeAll;
- (OSLogEntry *) fetchNextEntry;
- (NSString *) stringFromDarwinOSLogEntry:(OSLogEntry *)nextLogEntry;

@end

#endif // DARWINOS_SOURCE_OSLOG_H_INCLUDED