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

#include "contextual-data-record-scanner.h"
#include "csv-contextual-data-record-scanner.h"
#include "messages.h"
#include <string.h>

void
contextual_data_record_scanner_free(ContextualDataRecordScanner *self)
{
  if (self && self->free_fn)
    self->free_fn(self);
}

void
contextual_data_record_scanner_set_name_prefix(ContextualDataRecordScanner *
                                               self, const gchar *prefix)
{
  self->name_prefix = prefix;
}

ContextualDataRecordScanner *
create_contextual_data_record_scanner_by_type(GlobalConfig *cfg, const gchar *filename, const gchar *type)
{
  ContextualDataRecordScanner *scanner = NULL;

  if (type == NULL)
    return NULL;

  if (!strcmp(type, "csv"))
    {
      scanner = csv_contextual_data_record_scanner_new(cfg, filename);
    }

  if (!scanner)
    msg_warning("Unknown ContextualDataRecordScanner",
                evt_tag_str("type", type));


  return scanner;
}

ContextualDataRecord *
contextual_data_record_scanner_get_next(ContextualDataRecordScanner *self, const gchar *input)
{
  if (!self->get_next)
    return NULL;

  contextual_data_record_init(&self->last_record);
  if (!self->get_next(self, input, &self->last_record))
    {
      contextual_data_record_clean(&self->last_record);
      return NULL;
    }

  return &self->last_record;
}

void
contextual_data_record_scanner_init(ContextualDataRecordScanner *self, GlobalConfig *cfg)
{
  self->cfg = cfg;
}
