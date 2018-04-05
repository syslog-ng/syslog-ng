/*
 * Copyright (c) 2016 Balabit
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

#ifndef CONTEXTUAL_DATA_RECORD_SCANNER_H_INCLUDED
#define CONTEXTUAL_DATA_RECORD_SCANNER_H_INCLUDED

#include "syslog-ng.h"

typedef struct _ContextualDataRecordScanner ContextualDataRecordScanner;

typedef struct _ContextualDataRecord
{
  GString *selector;
  GString *name;
  GString *value;
} ContextualDataRecord;

struct _ContextualDataRecordScanner
{
  ContextualDataRecord last_record;
  gpointer scanner;
  const gchar *name_prefix;
  gboolean (*get_next) (ContextualDataRecordScanner *self,
                        const gchar *input,
                        ContextualDataRecord *record);
  void (*free_fn) (ContextualDataRecordScanner *self);
};


void contextual_data_record_scanner_free(ContextualDataRecordScanner *self);

void
contextual_data_record_scanner_set_name_prefix(ContextualDataRecordScanner *
                                               self, const gchar *prefix);

void contextual_data_record_init(ContextualDataRecord *record);
void contextual_data_record_clean(ContextualDataRecord *record);

ContextualDataRecordScanner
*create_contextual_data_record_scanner_by_type(const gchar *type);

ContextualDataRecord *
contextual_data_record_scanner_get_next(ContextualDataRecordScanner *self, const gchar *input);

#endif
