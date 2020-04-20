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
 */
#include "linux-audit-parser.h"
#include "scanner/kv-scanner/kv-scanner.h"
#include "utf8utils.h"

#include <string.h>
#include <ctype.h>

const gchar *hexcoded_fields[] =
{
  "name",
  "proctitle",
  "path",
  "dir",
  "comm",
  "ocomm",
  "data",
  "old",
  "new",
  NULL
};

static gint
_decode_xdigit(gchar xdigit)
{
  if (xdigit >= '0' && xdigit <= '9')
    return xdigit - '0';
  xdigit = toupper(xdigit);
  if (xdigit >= 'A' && xdigit <= 'F')
    return xdigit - 'A' + 10;
  return -1;
}

static gint
_decode_xbyte(gchar xdigit1, gchar xdigit2)
{
  gint nibble_hi, nibble_lo;

  nibble_hi = _decode_xdigit(xdigit1);
  nibble_lo = _decode_xdigit(xdigit2);
  if (nibble_hi < 0 || nibble_lo < 0)
    return -1;
  return (nibble_hi << 4) + nibble_lo;
}

static gboolean
_is_control_char(gint ch)
{
  if (ch < 0x21 || ch > 0x7e)
    return TRUE;
  return FALSE;
}

static gboolean
_parse_linux_audit_hexstring(GString *decoded_value, const gchar *value, gsize len)
{
  gint src;
  gboolean kernel_would_have_encoded_this_as_hex = FALSE;

  for (src = 0; src < len; src += 2)
    {
      gint v;

      v = _decode_xbyte(value[src], value[src + 1]);

      if (v < 0)
        return FALSE;

      if (_is_control_char(v) || v == '"')
        kernel_would_have_encoded_this_as_hex = TRUE;

      if (v == 0)
        v = '\t';

      g_string_append_c(decoded_value, v);
    }
  if (!kernel_would_have_encoded_this_as_hex)
    return FALSE;
  return TRUE;
}

static gboolean
_is_field_hex_encoded(const gchar *field)
{
  gint i;

  if (field[0] == 'a' && isdigit(field[1]))
    return TRUE;

  for (i = 0; hexcoded_fields[i]; i++)
    {
      if (strcmp(hexcoded_fields[i], field) == 0)
        return TRUE;
    }
  return FALSE;
}

gboolean
parse_linux_audit_style_hexdump(KVScanner *self)
{
  if (!self->value_was_quoted &&
      (self->value->len % 2) == 0 &&
      isxdigit(self->value->str[0]) &&
      _is_field_hex_encoded(self->key->str))
    {
      if (!_parse_linux_audit_hexstring(self->decoded_value, self->value->str, self->value->len))
        return FALSE;

      if (!g_utf8_validate(self->decoded_value->str, self->decoded_value->len, NULL))
        return FALSE;

      return TRUE;
    }
  return FALSE;
}

static void
_init_scanner(KVParser *self, KVScanner *kv_scanner)
{
  kv_parser_init_scanner_method(self, kv_scanner);
  kv_scanner_set_transform_value(kv_scanner, parse_linux_audit_style_hexdump);
}

static LogPipe *
_clone(LogPipe *s)
{
  KVParser *self = (KVParser *) s;
  KVParser *cloned = (KVParser *) linux_audit_parser_new(s->cfg);

  return kv_parser_clone_method(cloned, self);
}

LogParser *
linux_audit_parser_new(GlobalConfig *cfg)
{
  KVParser *self = g_new0(KVParser, 1);

  kv_parser_init_instance(self, cfg);
  self->super.super.clone = _clone;
  self->init_scanner = _init_scanner;
  return &self->super;
}
