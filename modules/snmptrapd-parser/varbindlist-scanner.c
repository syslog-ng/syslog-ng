/*
 * Copyright (c) 2017 Balabit
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

#include "varbindlist-scanner.h"
#include "str-utils.h"

static inline gboolean
_is_valid_key_character(gchar c)
{
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') ||
         (c == '_') ||
         (c == '.') ||
         (c == '-') ||
         (c == ':');
}

static inline void
_skip_whitespaces(const gchar **input)
{
  const gchar *current_char = *input;

  while (*current_char == ' ' || *current_char == '\t')
    ++current_char;

  *input = current_char;
}

static void
_extract_type(KVScanner *scanner)
{
  VarBindListScanner *self = (VarBindListScanner *) scanner;

  const gchar *type_start = &scanner->input[scanner->input_pos];
  _skip_whitespaces(&type_start);
  const gchar *type_end = strpbrk(type_start, ": \t");

  gboolean type_exists = type_end && *type_end == ':';
  if (!type_exists)
    {
      g_string_truncate(self->varbind_type, 0);
      return;
    }

  gsize type_len = type_end - type_start;
  g_string_assign_len(self->varbind_type, type_start, type_len);

  scanner->input_pos = type_end - scanner->input + 1;
}

void
varbindlist_scanner_init(VarBindListScanner *self)
{
  memset(self, 0, sizeof(VarBindListScanner));
  kv_scanner_init(&self->super, '=', "\t", FALSE);
  kv_scanner_set_extract_annotation_func(&self->super, _extract_type);
  kv_scanner_set_valid_key_character_func(&self->super, _is_valid_key_character);
  kv_scanner_set_stop_character(&self->super, '\n');

  self->varbind_type = g_string_sized_new(16);
}

void
varbindlist_scanner_deinit(VarBindListScanner *self)
{
  g_string_free(self->varbind_type, TRUE);
  kv_scanner_deinit(&self->super);
}

gboolean
varbindlist_scanner_scan_next(VarBindListScanner *self)
{
  return kv_scanner_scan_next(&self->super);
}

VarBindListScanner *
varbindlist_scanner_new(void)
{
  VarBindListScanner *self = g_new(VarBindListScanner, 1);
  varbindlist_scanner_init(self);
  return self;
}
