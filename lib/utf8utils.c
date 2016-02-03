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
#include "utf8utils.h"
#include "str-utils.h"

static inline gboolean
_is_character_unsafe(gunichar uchar, const gchar *unsafe_chars)
{
  if (uchar >= 256)
    return FALSE;

  if (!unsafe_chars)
    return FALSE;

  return _strchr_optimized_for_single_char_haystack(unsafe_chars, (gchar) uchar) != NULL;
}

static inline void
_append_unichar(GString *string, gunichar wc)
{
  if (wc < 0xc0)
    g_string_append_c(string, (gchar) wc);
  else
    g_string_append_unichar(string, wc);
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
 * Here are the rules that the routine follows:
 *   - well-known control characters are escaped (0x0a as \n and so on)
 *   - other control characters as per control_format
 *   - backslash is escaped as \\
 *   - any additional characters (only ASCII is supported) as \<char>
 *   - invalid utf8 sequences are converted as per invalid_format
 *   - utf8 characters are reproduced as is
 */
static gsize
_append_escaped_utf8_character(GString *escaped_output, const gchar **raw,
                               gssize raw_len, const gchar *unsafe_chars,
                               const gchar *control_format,
                               const gchar *invalid_format)
{
  const gchar *char_ptr = *raw;
  gunichar uchar = g_utf8_get_char_validated(char_ptr, raw_len);
  switch (uchar)
    {
      case (gunichar) -1:
      case (gunichar) -2:
        g_string_append_printf(escaped_output, invalid_format, *(guint8 *) char_ptr);
        (*raw)++;
        return 1;
        break;
      case '\b':
        g_string_append(escaped_output, "\\b");
        break;
      case '\f':
        g_string_append(escaped_output, "\\f");
        break;
      case '\n':
        g_string_append(escaped_output, "\\n");
        break;
      case '\r':
        g_string_append(escaped_output, "\\r");
        break;
      case '\t':
        g_string_append(escaped_output, "\\t");
        break;
      case '\\':
        g_string_append(escaped_output, "\\\\");
        break;
      default:
        if (uchar < 32)
          g_string_append_printf(escaped_output, control_format, uchar);
        else if (_is_character_unsafe(uchar, unsafe_chars))
          g_string_append_printf(escaped_output, "\\%c", (gchar) uchar);
        else
          _append_unichar(escaped_output, uchar);
        break;
    }
  *raw = g_utf8_next_char(char_ptr);
  return *raw - char_ptr;
}

/**
 * @see _append_escaped_utf8_character()
 */
static void
_append_unsafe_utf8_as_escaped(GString *escaped_output, const gchar *raw,
                               gssize raw_len, const gchar *unsafe_chars,
                               const gchar *control_format,
                               const gchar *invalid_format)
{
  if (raw_len < 0)
      while (*raw)
        _append_escaped_utf8_character(escaped_output, &raw, -1, unsafe_chars,
                                       control_format, invalid_format);
  else
      while (raw_len)
        raw_len -= _append_escaped_utf8_character(escaped_output, &raw, raw_len, unsafe_chars,
                                                  control_format, invalid_format);
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
 * Here are the rules that the routine follows:
 *   - well-known control characters are escaped (0x0a as \n and so on)
 *   - other control characters as per control_format (\xXX)
 *   - backslash is escaped as \\
 *   - any additional characters (only ASCII is supported) as \<char>
 *   - invalid utf8 sequences are converted as per invalid_format (\xXX)
 *   - utf8 characters are reproduced as is
 *
 * This is basically meant to be used when sending data to
 * 8 bit clean receivers, e.g. syslog-ng or WELF.
 * @see _append_unsafe_utf8_as_escaped()
 */
void
append_unsafe_utf8_as_escaped_binary(GString *escaped_string, const gchar *str,
                                     gssize str_len, const gchar *unsafe_chars)
{
  _append_unsafe_utf8_as_escaped(escaped_string, str, str_len, unsafe_chars,
                                 "\\x%02x", "\\x%02x");
}
gchar *
convert_unsafe_utf8_to_escaped_binary(const gchar *str, gssize str_len,
                                      const gchar *unsafe_chars)
{
  if (str_len < 0)
    str_len = strlen(str);
  GString *escaped_string = g_string_sized_new(str_len);

  append_unsafe_utf8_as_escaped_binary(escaped_string, str, str_len, unsafe_chars);
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
 * Here are the rules that the routine follows:
 *   - well-known control characters are escaped (0x0a as \n and so on)
 *   - other control characters as per control_format (\u00XX)
 *   - backslash is escaped as \\
 *   - any additional characters (only ASCII is supported) as \<char>
 *   - invalid utf8 sequences are converted as per invalid_format (\\xXX)
 *   - utf8 characters are reproduced as is
 *
 * This is basically meant to be used when sending data to
 * utf8 only receivers, e.g. JSON.
 * @see _append_unsafe_utf8_as_escaped()
 */
void
append_unsafe_utf8_as_escaped_text(GString *escaped_string, const gchar *str,
                                    gssize str_len, const gchar *unsafe_chars)
{
  _append_unsafe_utf8_as_escaped(escaped_string, str, str_len, unsafe_chars,
                                 "\\u%04x", "\\\\x%02x");
}

gchar *
convert_unsafe_utf8_to_escaped_text(const gchar *str, gssize str_len,
                                    const gchar *unsafe_chars)
{
  if (str_len < 0)
    str_len = strlen(str);
  GString *escaped_string = g_string_sized_new(str_len);

  append_unsafe_utf8_as_escaped_text(escaped_string, str, str_len, unsafe_chars);
  return g_string_free(escaped_string, FALSE);
}
