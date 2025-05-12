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
_is_character_unsafe(gunichar uchar, guint32 unsafe_flags)
{
  return (uchar == '"' && (unsafe_flags & AUTF8_UNSAFE_QUOTE)) ||
         (uchar == '\'' && (unsafe_flags & AUTF8_UNSAFE_APOSTROPHE));
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
static inline gsize
_append_escaped_utf8_character_not_ascii(GString *escaped_output, const gchar **raw,
                                         gssize raw_len, guint32 unsafe_flags,
                                         const gchar *control_format,
                                         const gchar *invalid_format)
{
  const gchar *char_ptr = *raw;
  gunichar uchar = g_utf8_get_char_validated(char_ptr, raw_len);

  if (G_UNLIKELY(uchar == (gunichar) -1 || uchar == (gunichar) -2))
    {
      g_string_append_printf(escaped_output, invalid_format, *(guint8 *) char_ptr);
      (*raw)++;
      return 1;
    }
  else if (G_UNLIKELY(uchar < 32 || uchar == '\\'))
    {
      switch (uchar)
        {
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
          g_string_append_printf(escaped_output, control_format, uchar);
          break;
        }
    }
  else if (G_UNLIKELY(_is_character_unsafe(uchar, unsafe_flags)))
    g_string_append_printf(escaped_output, "\\%c", (gchar) uchar);
  else
    g_string_append_unichar_optimized(escaped_output, uchar);

  *raw = g_utf8_next_char(char_ptr);
  return *raw - char_ptr;
}

static inline gsize
_append_escaped_utf8_character_ascii(GString *escaped_output, const gchar **raw,
                                     gssize raw_len, guint32 unsafe_flags,
                                     const gchar *control_format,
                                     const gchar *invalid_format)
{
  const gchar *char_ptr = *raw;
  gchar achar = *char_ptr;

  if (G_UNLIKELY(achar < 32 || achar == '\\'))
    {
      switch (achar)
        {
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
          g_string_append_printf(escaped_output, control_format, achar);
          break;
        }
    }
  else if (G_UNLIKELY(_is_character_unsafe(achar, unsafe_flags)))
    g_string_append_printf(escaped_output, "\\%c", (gchar) achar);
  else
    g_string_append_c(escaped_output, achar);

  *raw = char_ptr + 1;
  return 1;
}

static inline gsize
_append_escaped_utf8_character(GString *escaped_output, const gchar **raw,
                               gssize raw_len, guint32 unsafe_flags,
                               const gchar *control_format,
                               const gchar *invalid_format)
{
  if (**raw > 0 && **raw <= 127)
    return _append_escaped_utf8_character_ascii(escaped_output, raw, raw_len, unsafe_flags, control_format, invalid_format);
  else
    return _append_escaped_utf8_character_not_ascii(escaped_output, raw, raw_len, unsafe_flags, control_format,
                                                    invalid_format);
}

static inline void
_append_unsafe_utf8_as_escaped_with_specific_length(GString *escaped_output, const gchar *raw,
                                                    gsize raw_len,
                                                    guint32 unsafe_flags,
                                                    const gchar *control_format,
                                                    const gchar *invalid_format)
{
  const gchar *raw_end = raw + raw_len;

  while (raw < raw_end)
    _append_escaped_utf8_character(escaped_output, &raw, raw_end - raw, unsafe_flags,
                                   control_format, invalid_format);
}

static inline void
_append_unsafe_utf8_as_escaped_nul_terminated(GString *escaped_output, const gchar *raw,
                                              guint32 unsafe_flags,
                                              const gchar *control_format,
                                              const gchar *invalid_format)
{
  _append_unsafe_utf8_as_escaped_with_specific_length(escaped_output, raw, strlen(raw), unsafe_flags, control_format,
                                                      invalid_format);
}


/**
 * @see _append_escaped_utf8_character()
 */
void
append_unsafe_utf8_as_escaped(GString *escaped_output, const gchar *raw,
                              gssize raw_len, guint32 unsafe_flags,
                              const gchar *control_format,
                              const gchar *invalid_format)
{
  if (raw_len < 0)
    _append_unsafe_utf8_as_escaped_nul_terminated(escaped_output, raw, unsafe_flags, control_format, invalid_format);
  else
    _append_unsafe_utf8_as_escaped_with_specific_length(escaped_output, raw, raw_len, unsafe_flags, control_format,
                                                        invalid_format);
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
 * @see append_unsafe_utf8_as_escaped()
 */
void
append_unsafe_utf8_as_escaped_binary(GString *escaped_string, const gchar *str,
                                     gssize str_len, guint32 unsafe_flags)
{
  append_unsafe_utf8_as_escaped(escaped_string, str, str_len, unsafe_flags,
                                "\\x%02x", "\\x%02x");
}

gchar *
convert_unsafe_utf8_to_escaped_binary(const gchar *str, gssize str_len,
                                      guint32 unsafe_flags)
{
  if (str_len < 0)
    str_len = strlen(str);
  GString *escaped_string = g_string_sized_new(str_len);

  append_unsafe_utf8_as_escaped_binary(escaped_string, str, str_len, unsafe_flags);
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
 *   - other control characters as per control_format (\xXX)
 *   - backslash is escaped as \\
 *   - any additional characters (only ASCII is supported) as \<char>
 *   - invalid utf8 sequences are converted as per invalid_format (\\xXX)
 *   - utf8 characters are reproduced as is
 *
 * This is basically meant to be used when sending data to
 * utf8 only receivers, e.g. JSON.
 * @see append_unsafe_utf8_as_escaped()
 */
void
append_unsafe_utf8_as_escaped_text(GString *escaped_string, const gchar *str,
                                   gssize str_len, guint32 unsafe_flags)
{
  append_unsafe_utf8_as_escaped(escaped_string, str, str_len, unsafe_flags,
                                "\\x%02x", "\\\\x%02x");
}

gchar *
convert_unsafe_utf8_to_escaped_text(const gchar *str, gssize str_len,
                                    guint32 unsafe_flags)
{
  if (str_len < 0)
    str_len = strlen(str);
  GString *escaped_string = g_string_sized_new(str_len);

  append_unsafe_utf8_as_escaped_text(escaped_string, str, str_len, unsafe_flags);
  return g_string_free(escaped_string, FALSE);
}
