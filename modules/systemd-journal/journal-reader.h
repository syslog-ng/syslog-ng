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

#ifndef JOURNAL_READER_H_
#define JOURNAL_READER_H_

#include "logsource.h"
#include "journald-subsystem.h"

typedef struct _JournalReader JournalReader;

typedef struct _JournalReaderOptions
{
  LogSourceOptions super;
  gboolean initialized;
  gint fetch_limit;
  guint16 default_pri;
  guint32 flags;
  gchar *recv_time_zone;
  TimeZoneInfo *recv_time_zone_info;
  gchar *prefix;
  guint32 max_field_size;
  gchar *namespace;
} JournalReaderOptions;

JournalReader *journal_reader_new(GlobalConfig *cfg, Journald *journal);
void journal_reader_set_options(LogPipe *s, LogPipe *control, JournalReaderOptions *options, const gchar *stats_id,
                                const gchar *stats_instance);

void journal_reader_options_init(JournalReaderOptions *options, GlobalConfig *cfg, const gchar *group_name);
void journal_reader_options_set_default_severity(JournalReaderOptions *self, gint severity);
void journal_reader_options_set_default_facility(JournalReaderOptions *self, gint facility);
void journal_reader_options_set_time_zone(JournalReaderOptions *self, gchar *time_zone);
void journal_reader_options_set_prefix(JournalReaderOptions *self, gchar *prefix);
void journal_reader_options_set_max_field_size(JournalReaderOptions *self, gint max_field_size);
void journal_reader_options_set_namespace(JournalReaderOptions *self, gchar *namespace);
void journal_reader_options_set_log_fetch_limit(JournalReaderOptions *self, gint log_fetch_limit);
void journal_reader_options_defaults(JournalReaderOptions *options);
void journal_reader_options_destroy(JournalReaderOptions *options);

#endif /* JOURNAL_READER_H_ */
