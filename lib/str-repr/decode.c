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
  KV_FINISH_SUCCESS,
  KV_FINISH_FAILURE,
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

typedef struct _StrReprDecodeState
{
  GString *value;
  const gchar *cur;
  const gchar *closing_delimiter;
  gchar quote_char;
  const StrReprDecodeOptions *options;
} StrReprDecodeState;

static gboolean
_invoke_match_delimiter(StrReprDecodeState *state, const gchar **new_cur)
{
  const StrReprDecodeOptions *options = state->options;

  if (options->delimiter_chars[0])
    {
      if (*state->cur == options->delimiter_chars[0] || *state->cur == options->delimiter_chars[1]
          || *state->cur == options->delimiter_chars[2])
        {
          if (options->match_delimiter)
            return options->match_delimiter(state->cur, new_cur, options->match_delimiter_data);
          else
            {
              *new_cur = state->cur + 1;
              return TRUE;
            }
        }
    }
  else
    {
      if (options->match_delimiter)
        return options->match_delimiter(state->cur, new_cur, options->match_delimiter_data);
    }
  return FALSE;
}

static gboolean
_match_and_skip_delimiter(StrReprDecodeState *state)
{
  const gchar *new_cur;

  if (_invoke_match_delimiter(state, &new_cur))
    {
      state->cur = new_cur;
      return TRUE;
    }
  return FALSE;
}


static gint
_process_initial_character(StrReprDecodeState *state)
{
  if (_match_and_skip_delimiter(state))
    {
      return KV_FINISH_SUCCESS;
    }
  else if (*state->cur == '\"' || *state->cur == '\'')
    {
      state->quote_char = *state->cur;
      return KV_QUOTE_STRING;
    }
  else
    {
      g_string_append_c(state->value, *state->cur);
      return KV_UNQUOTED_CHARACTERS;
    }
}

static gint
_process_quoted_string_characters(StrReprDecodeState *state)
{
  if (*state->cur == state->quote_char)
    return KV_EXPECT_DELIMITER;
  else if (*state->cur == '\\')
    return KV_QUOTE_BACKSLASH;

  g_string_append_c(state->value, *state->cur);
  return KV_QUOTE_STRING;
}

static gint
_process_backslash_escaped_character_in_strings(StrReprDecodeState *state)
{
  _decode_backslash_escape(state->value, state->quote_char, *state->cur);
  return KV_QUOTE_STRING;
}

static gint
_process_delimiter_characters_after_a_quoted_string(StrReprDecodeState *state)
{
  state->closing_delimiter = state->cur;
  if (_match_and_skip_delimiter(state))
    return KV_FINISH_SUCCESS;
  return KV_QUOTE_ERROR;
}

static gint
_process_junk_after_closing_quote(StrReprDecodeState *state)
{
  if (_match_and_skip_delimiter(state))
    return KV_FINISH_FAILURE;
  return KV_QUOTE_ERROR;
}

static gint
_process_unquoted_characters(StrReprDecodeState *state)
{
  if (_match_and_skip_delimiter(state))
    return KV_FINISH_SUCCESS;
  g_string_append_c(state->value, *state->cur);
  return KV_UNQUOTED_CHARACTERS;
}

static gboolean
_is_an_ending_string_acceptable_in_this_state(gint quote_state)
{
  return quote_state == KV_QUOTE_INITIAL ||
         quote_state == KV_EXPECT_DELIMITER ||
         quote_state == KV_UNQUOTED_CHARACTERS ||
         quote_state == KV_FINISH_SUCCESS;
}

static gboolean
_decode(StrReprDecodeState *state)
{
  gint quote_state = KV_QUOTE_INITIAL;

  for (; *state->cur; state->cur++)
    {
      switch (quote_state)
        {
        case KV_QUOTE_INITIAL:
          quote_state = _process_initial_character(state);
          break;
        case KV_QUOTE_STRING:
          quote_state = _process_quoted_string_characters(state);
          break;
        case KV_QUOTE_BACKSLASH:
          quote_state = _process_backslash_escaped_character_in_strings(state);
          break;
        case KV_EXPECT_DELIMITER:
          quote_state = _process_delimiter_characters_after_a_quoted_string(state);
          break;
        case KV_QUOTE_ERROR:
          quote_state = _process_junk_after_closing_quote(state);
          break;
        case KV_UNQUOTED_CHARACTERS:
          quote_state = _process_unquoted_characters(state);
          break;
        default:
          break;
        }
      if (quote_state == KV_FINISH_SUCCESS || quote_state == KV_FINISH_FAILURE)
        break;
    }
  if (_is_an_ending_string_acceptable_in_this_state(quote_state))
    return TRUE;
  return FALSE;
}

gboolean
str_repr_decode_append_with_options(GString *value, const gchar *input, const gchar **end,
                                    const StrReprDecodeOptions *options)
{
  StrReprDecodeState state =
  {
    .value = value,
    .cur = input,
    .closing_delimiter = 0,
    .quote_char = 0,
    .options = options,
  };
  gsize initial_len = value->len;

  gboolean success = _decode(&state);
  *end = state.cur;

  if (!success)
    {
      if (state.closing_delimiter)
        {
          g_string_truncate(value, initial_len);
          g_string_append_len(value, input, state.closing_delimiter - input);
        }
      else
        {
          g_string_truncate(value, initial_len);
          g_string_append_len(value, input, state.cur - input);
        }

      return FALSE;
    }
  return TRUE;
}

gboolean
str_repr_decode_append(GString *value, const gchar *input, const gchar **end)
{
  StrReprDecodeOptions options =
  {
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
