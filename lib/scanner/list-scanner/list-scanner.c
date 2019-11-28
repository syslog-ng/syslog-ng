/*
 * Copyright (c) 2016 Balabit
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "scanner/list-scanner/list-scanner.h"
#include "str-repr/encode.h"
#include "str-repr/decode.h"
#include "str-utils.h"

#include <string.h>
#include <stdarg.h>

static void
_free_input(ListScanner *self)
{
  if (self->argv)
    {
      if (self->free_argv_payload)
        g_ptr_array_foreach(self->argv_buffer, (GFunc) g_free, NULL);
    }
  g_ptr_array_set_size(self->argv_buffer, 0);
}

static void
_store_input(ListScanner *self, gint argc, gchar **argv, gboolean free_argv)
{
  self->argc = argc;
  self->argv = (gchar **) self->argv_buffer->pdata;
  self->free_argv_payload = free_argv;
  self->current_arg_ndx = 0;
  self->current_arg = self->argv[self->current_arg_ndx];
}

void
list_scanner_input_va(ListScanner *self, const gchar *arg1, ...)
{
  va_list va;
  gint i;
  const char *arg;

  _free_input(self);

  va_start(va, arg1);
  for (i = 0, arg = arg1; arg; i++, arg = va_arg(va, const gchar *))
    {
      g_ptr_array_add(self->argv_buffer, g_strdup(arg));
    }
  g_ptr_array_add(self->argv_buffer, NULL);
  va_end(va);
  _store_input(self, i, (gchar **) self->argv_buffer->pdata, TRUE);
}

void
list_scanner_input_gstring_array(ListScanner *self, gint argc, GString *const *argv)
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      g_ptr_array_add(self->argv_buffer, argv[i]->str);
    }
  g_ptr_array_add(self->argv_buffer, NULL);
  _store_input(self, argc, (gchar **) self->argv_buffer->pdata, FALSE);
}

void
list_scanner_input_string(ListScanner *self, const gchar *value, gssize value_len)
{
  _free_input(self);
  if (value_len < 0)
    value_len = strlen(value);
  g_ptr_array_add(self->argv_buffer, g_strndup(value, value_len));
  g_ptr_array_add(self->argv_buffer, NULL);
  _store_input(self, 1, (gchar **) self->argv_buffer->pdata, TRUE);
}

static gboolean
_is_finished_with_args(ListScanner *self)
{
  return self->current_arg_ndx >= self->argc;
}

static gboolean
_parse_value_from_current_position(ListScanner *self)
{
  StrReprDecodeOptions options =
  {
    0,
    .delimiter_chars = ",",
  };
  const gchar *end;

  /* NOTE: we always try to parse elements, even if the string is malformed,
   * otherwise we would be losing data.  Prefer to have data in an
   * incorrectly formatted way, than no data at all. */
  if (!str_repr_decode_with_options(self->value, self->current_arg, &end, &options))
    {
      g_string_truncate(self->value, 0);
      g_string_append_len(self->value, self->current_arg, end - self->current_arg);
    }
  self->current_arg = end;

  return TRUE;
}

static gboolean
_skip_if_current_arg_is_empty(ListScanner *self)
{
  if (self->current_arg[0] == 0)
    {
      self->current_arg_ndx++;
      self->current_arg = self->argv[self->current_arg_ndx];
      return TRUE;
    }
  return FALSE;
}

static gboolean
_skip_unquoted_empty_value(ListScanner *self)
{
  if (*self->current_arg == ',')
    {
      self->current_arg = self->current_arg + 1;
      return TRUE;
    }
  return FALSE;
}

static void
_skip_empty_values(ListScanner *self)
{
  while (!_is_finished_with_args(self))
    {
      if (!_skip_if_current_arg_is_empty(self) &&
          !_skip_unquoted_empty_value(self))
        break;
    }
  return;
}

gboolean
list_scanner_scan_next(ListScanner *self)
{
  g_string_truncate(self->value, 0);

  _skip_empty_values(self);

  if (!_is_finished_with_args(self) &&
      _parse_value_from_current_position(self))
    return TRUE;

  return FALSE;
}

void
list_scanner_skip_n(ListScanner *self, gint n)
{
  gint count = 0;
  while (++count <= n && list_scanner_scan_next(self))
    ;
}

const gchar *
list_scanner_get_current_value(ListScanner *self)
{
  return self->value->str;
}

gsize
list_scanner_get_current_value_len(ListScanner *self)
{
  return self->value->len;
}

void
list_scanner_init(ListScanner *self)
{
  memset(self, 0, sizeof(*self));
  self->value = g_string_sized_new(32);
  self->argv_buffer = g_ptr_array_new();
}

void
list_scanner_deinit(ListScanner *self)
{
  _free_input(self);
  g_string_free(self->value, TRUE);
  g_ptr_array_free(self->argv_buffer, TRUE);
}

ListScanner *
list_scanner_new(void)
{
  ListScanner *self = g_new0(ListScanner, 1);

  list_scanner_init(self);
  return self;
}

void
list_scanner_free(ListScanner *self)
{
  list_scanner_deinit(self);
  g_free(self);
}
