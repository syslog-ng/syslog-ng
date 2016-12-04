/*
 * Copyright (c) 2015-2016 Balabit
 * Copyright (c) 2015-2016 Balázs Scheidler
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
#include "str-repr/decode.h"

#include <string.h>

enum
{
  KV_QUOTE_INITIAL = 0,
  KV_QUOTE_STRING,
  KV_QUOTE_BACKSLASH,
  KV_QUOTE_FINISH,
  KV_EXPECT_DELIMITER,
  KV_QUOTE_ERROR,
  KV_UNQUOTED_CHARACTERS,
};

static void
_decode_backslash_escape(GString *value, gchar quote_char, gchar ch)
{
  gchar control;
  switch (ch)
    {
    case 'b':
      control = '\b';
      break;
    case 'f':
      control = '\f';
      break;
    case 'n':
      control = '\n';
      break;
    case 'r':
      control = '\r';
      break;
    case 't':
      control = '\t';
      break;
    case '\\':
      control = '\\';
      break;
    default:
      if (quote_char != ch)
        g_string_append_c(value, '\\');
      control = ch;
      break;
    }
  g_string_append_c(value, control);
}

static inline gboolean
_invoke_match_delimiter(const gchar *cur, const gchar **new_cur, const StrReprDecodeOptions *options)
{
  if (options->delimiter_chars[0])
    {
      if (*cur == options->delimiter_chars[0] || *cur == options->delimiter_chars[1])
        {
          if (options->match_delimiter)
            return options->match_delimiter(cur, new_cur, options->match_delimiter_data);
          else
            {
              *new_cur = cur + 1;
              return TRUE;
            }
        }
    }
  else
    {
      if (options->match_delimiter)
        return options->match_delimiter(cur, new_cur, options->match_delimiter_data);
    }
  return FALSE;
}

gboolean
str_repr_decode_append_with_options(GString *value, const gchar *input, const gchar **end, const StrReprDecodeOptions *options)
{
  const gchar *cur = input, *new_cur;
  gchar quote_char;
  gint quote_state;
  gsize initial_len = value->len;

  quote_state = KV_QUOTE_INITIAL;
  while (*cur)
    {
      switch (quote_state)
        {
        case KV_QUOTE_INITIAL:
          if (_invoke_match_delimiter(cur, &new_cur, options))
            {
              cur = new_cur;
              goto finish;
            }
          else if (*cur == '\"' || *cur == '\'')
            {
              quote_state = KV_QUOTE_STRING;
              quote_char = *cur;
            }
          else
            {
              g_string_append_c(value, *cur);
              quote_state = KV_UNQUOTED_CHARACTERS;
            }
          break;
        case KV_QUOTE_STRING:
          if (*cur == quote_char)
            quote_state = KV_EXPECT_DELIMITER;
          else if (*cur == '\\')
            quote_state = KV_QUOTE_BACKSLASH;
          else
            g_string_append_c(value, *cur);
          break;
        case KV_QUOTE_BACKSLASH:
          _decode_backslash_escape(value, quote_char, *cur);
          quote_state = KV_QUOTE_STRING;
          break;
        case KV_EXPECT_DELIMITER:
          if (_invoke_match_delimiter(cur, &new_cur, options))
            {
              cur = new_cur;
              quote_state = KV_QUOTE_INITIAL;
              goto finish;
            }
          else
            {
              quote_state = KV_QUOTE_ERROR;
            }
          break;
        case KV_QUOTE_ERROR:
          if (_invoke_match_delimiter(cur, &new_cur, options))
            {
              cur = new_cur;
              goto finish;
            }
          break;
        case KV_UNQUOTED_CHARACTERS:
          if (_invoke_match_delimiter(cur, &new_cur, options))
            {
              cur = new_cur;
              goto finish;
            }
          else
            {
              g_string_append_c(value, *cur);
            }
          break;
        }
      cur++;
    }
 finish:
  *end = cur;
  /* check if quotation was not finished or we had extra characters, return FALSE */
  if (quote_state == KV_QUOTE_INITIAL ||
      quote_state == KV_EXPECT_DELIMITER ||
      quote_state == KV_UNQUOTED_CHARACTERS)
    {
      return TRUE;
    }
  else
    {
      g_string_truncate(value, initial_len);
      g_string_append_len(value, input, cur - input);
      return FALSE;
    }
}

gboolean
str_repr_decode_append(GString *value, const gchar *input, const gchar **end)
{
  StrReprDecodeOptions options = {
    0,
    .delimiter_chars = " ",
  };
  return str_repr_decode_append_with_options(value, input, end, &options);
}

gboolean
str_repr_decode(GString *value, const gchar *input, const gchar **end)
{
  g_string_truncate(value, 0);
  return str_repr_decode_append(value, input, end);
}

gboolean
str_repr_decode_with_options(GString *value, const gchar *input, const gchar **end, const StrReprDecodeOptions *options)
{
  g_string_truncate(value, 0);
  return str_repr_decode_append_with_options(value, input, end, options);
}
