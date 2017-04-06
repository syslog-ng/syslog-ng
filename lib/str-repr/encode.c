/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balázs Scheidler
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
#include "str-repr/encode.h"
#include "utf8utils.h"
#include <string.h>

void
str_repr_encode_append(GString *escaped_string, const gchar *str, gssize str_len, const gchar *forbidden_chars)
{
  const gchar *apostrophe, *quote;

  if (str_len < 0)
    str_len = strlen(str);

  if (str_len == 0)
    {
      g_string_append_len(escaped_string, "\"\"", 2);
      return;
    }

  apostrophe = memchr(str, '\'', str_len);
  quote = memchr(str, '"', str_len);

  if (!apostrophe && !quote)
    {
      gboolean quoting_needed = FALSE;

      /* NOTE: for non-NUL terminated strings, this would go over the end of
       * the string until the first NUL character.  It is not ideal at all,
       * but there's no strncspn() */

      if ((strcspn(str, "\b\f\n\r\t\\ ")) < str_len ||
          (forbidden_chars && strcspn(str, forbidden_chars) < str_len))
        quoting_needed = TRUE;

      if (!quoting_needed)
        {
          g_string_append_len(escaped_string, str, str_len);
          return;
        }
    }

  if (!apostrophe && quote)
    {
      g_string_append_c(escaped_string, '\'');
      append_unsafe_utf8_as_escaped_binary(escaped_string, str, str_len, NULL);
      g_string_append_c(escaped_string, '\'');
    }
  else if (!quote && apostrophe)
    {
      g_string_append_c(escaped_string, '"');
      append_unsafe_utf8_as_escaped_binary(escaped_string, str, str_len, NULL);
      g_string_append_c(escaped_string, '"');
    }
  else
    {
      g_string_append_c(escaped_string, '"');
      append_unsafe_utf8_as_escaped_binary(escaped_string, str, str_len, "\"");
      g_string_append_c(escaped_string, '"');
    }
}

void
str_repr_encode(GString *escaped_string, const gchar *str, gssize str_len, const gchar *forbidden_chars)
{
  g_string_truncate(escaped_string, 0);
  str_repr_encode_append(escaped_string, str, str_len, forbidden_chars);
}
