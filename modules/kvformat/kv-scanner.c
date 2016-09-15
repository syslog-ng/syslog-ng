/*
 * Copyright (c) 2015-2016 Balabit
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
#include "kv-scanner.h"

void
kv_scanner_init(KVScanner *self, gchar value_separator, KVParseValue *parse_value)
{
  memset(self, 0, sizeof(*self));
  self->key = g_string_sized_new(32);
  self->value = g_string_sized_new(64);
  self->decoded_value = g_string_sized_new(64);
  self->value_separator = value_separator;
  self->parse_value = parse_value;
}

void
kv_scanner_free(KVScanner *self)
{
  if (!self)
    return;
  g_string_free(self->key, TRUE);
  g_string_free(self->value, TRUE);
  g_string_free(self->decoded_value, TRUE);
  g_free(self);
}
