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

#include "tagrecord-scanner.h"
#include "csv-tagrecord-scanner.h"
#include "messages.h"
#include <string.h>

void
tag_record_scanner_free(TagRecordScanner *self)
{
  if (self && self->free_fn)
    self->free_fn(self);
}

void
tag_record_scanner_set_name_prefix(TagRecordScanner *self, const gchar *prefix)
{
  self->name_prefix = prefix;
}

void
tag_record_clean(TagRecord *record)
{
  g_free(record->selector);
  g_free(record->name);
  g_free(record->value);
}

TagRecordScanner*
create_tag_record_scanner_by_type(const gchar *type)
{
  TagRecordScanner *scanner = NULL;

  if (type == NULL)
    return NULL;

  if (!strncmp(type, "csv", 3))
    {
      scanner = csv_tag_record_scanner_new();
    }

  if (!scanner)
    msg_warning("Unknown TagRecordScanner", evt_tag_str("type", type));

  return scanner;
}
