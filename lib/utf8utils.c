/*
 * Copyright (c) 2015 BalaBit
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
#include "utf8utils.h"
#include <string.h>

/**
 * This function escapes an unsanitized input (e.g. that can contain binary
 * characters, and produces an escaped format that can be deescaped in need,
 * which is guaranteed to be utf8 clean.  The major difference between
 * "binary" and "text" form is that the receiver is able to cope with \xXX
 * sequences that can incorporate invalid utf8 sequences when decoded.  With
 * "text" format, we never embed anything that would become not valid utf8
 * when decoded.
 *
 * This is basically meant to be used when sending data to
 * 8 bit clean receivers, e.g. syslog-ng or WELF.
 *
 * Here are the rules that the routine follows:
 *   - well-known control characters are escaped (0x0a as \n and so on)
 *   - other control characters as \xXX
 *   - backslash is escaped as \\
 *   - any additional characters (only ASCII is supported) as \<char>
 *   - invalid utf8 sequences are converted to \xXX
 *   - utf8 characters are reproduced as is
 */
void
append_unsafe_utf8_as_escaped_binary(GString *escaped_string, const gchar *str, const gchar *unsafe_chars)
{
  const gchar *char_ptr = str;

  while (*char_ptr)
    {
      gunichar uchar = g_utf8_get_char_validated(char_ptr, -1);

      switch (uchar)
        {
          case (gunichar) -1:
            g_string_append_printf(escaped_string, "\\x%02x", *(guint8 *) char_ptr);
            char_ptr++;
            continue;
            break;
          case '\b':
            g_string_append(escaped_string, "\\b");
            break;
          case '\f':
            g_string_append(escaped_string, "\\f");
            break;
          case '\n':
            g_string_append(escaped_string, "\\n");
            break;
          case '\r':
            g_string_append(escaped_string, "\\r");
            break;
          case '\t':
            g_string_append(escaped_string, "\\t");
            break;
          case '\\':
            g_string_append(escaped_string, "\\\\");
            break;
          default:
            if (uchar < 32)
              g_string_append_printf(escaped_string, "\\x%02x", uchar);
            else if (uchar < 256 && unsafe_chars && strchr(unsafe_chars, (gchar) uchar))
              g_string_append_printf(escaped_string, "\\%c", (gchar) uchar);
            else
              g_string_append_unichar(escaped_string, uchar);
            break;
        }
      char_ptr = g_utf8_next_char(char_ptr);
    }
}

gchar *
convert_unsafe_utf8_to_escaped_binary(const gchar *str, const gchar *unsafe_chars)
{
  GString *escaped_string;

  escaped_string = g_string_sized_new(strlen(str));
  append_unsafe_utf8_as_escaped_binary(escaped_string, str, unsafe_chars);
  return g_string_free(escaped_string, FALSE);
}

/**
 * This function escapes an unsanitized input (e.g. that can contain binary
 * characters, and produces an escaped format that can be deescaped in need,
 * which is guaranteed to be utf8 clean.  The major difference between
 * "binary" and "text" form is that the receiver is able to cope with \xXX
 * sequences that can incorporate invalid utf8 sequences when decoded.  With
 * "text" format, we never embed anything that would become not valid utf8
 * when decoded.
 *
 * This is basically meant to be used when sending data to
 * utf8 only receivers, e.g. JSON.
 *
 * Here are the rules that the routine follows:
 *   - well-known control characters are escaped (0x0a as \n and so on)
 *   - other control characters as \u00XX
 *   - backslash is escaped as \\
 *   - any additional characters (only ASCII is supported) as \<char>
 *   - invalid utf8 sequences are converted to \\xXX (e.g. double-escaped as a literal \xXX sequence)
 *   - utf8 characters are reproduced as is
 **/
void
append_unsafe_utf8_as_escaped_text(GString *escaped_string, const gchar *str, const gchar *unsafe_chars)
{
  const gchar *char_ptr = str;

  while (*char_ptr)
    {
      gunichar uchar = g_utf8_get_char_validated(char_ptr, -1);

      switch (uchar)
        {
          case (gunichar) -1:
            g_string_append_printf(escaped_string, "\\\\x%02x", *(guint8 *) char_ptr);
            char_ptr++;
            continue;
            break;
          case '\b':
            g_string_append(escaped_string, "\\b");
            break;
          case '\f':
            g_string_append(escaped_string, "\\f");
            break;
          case '\n':
            g_string_append(escaped_string, "\\n");
            break;
          case '\r':
            g_string_append(escaped_string, "\\r");
            break;
          case '\t':
            g_string_append(escaped_string, "\\t");
            break;
          case '\\':
            g_string_append(escaped_string, "\\\\");
            break;
          default:
            if (uchar < 32)
              g_string_append_printf(escaped_string, "\\u%04x", uchar);
            else if (uchar < 256 && unsafe_chars && strchr(unsafe_chars, (gchar) uchar))
              g_string_append_printf(escaped_string, "\\%c", (gchar) uchar);
            else
              g_string_append_unichar(escaped_string, uchar);
            break;
        }
      char_ptr = g_utf8_next_char(char_ptr);
    }
}

gchar *
convert_unsafe_utf8_to_escaped_text(const gchar *str, const gchar *unsafe_chars)
{
  GString *escaped_string;

  escaped_string = g_string_sized_new(strlen(str));
  append_unsafe_utf8_as_escaped_text(escaped_string, str, unsafe_chars);
  return g_string_free(escaped_string, FALSE);
}
