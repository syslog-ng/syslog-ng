/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2013 Balázs Scheidler <balazs.scheidler@balabit.com>
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

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include <string.h>

/* NOTE: including the implementation file to access static functions */
#include "radix.c"

static gboolean
_invoke_parser(gboolean (*parser)(guint8 *str, gint *len, const gchar *param, gpointer state, RParserMatch *match),
               const gchar *str, gpointer param, gpointer state, gchar **result_string)
{
  gboolean result;
  gint len = 0;
  guint8 *dup = g_strdup(str);
  RParserMatch match;

  memset(&match, 0, sizeof(match));
  result = parser(dup, &len, param, state, &match);

  if (match.match)
    {
      if (result)
        *result_string = g_strdup(match.match);
    }
  else
    {
      match.ofs = 0 + match.ofs;
      match.len = (gint16) match.len + len;
      if (result)
        *result_string = g_strndup(&dup[match.ofs], match.len);
    }
  g_free(dup);
  return result;
}

typedef struct _parser_test_param
{
  gchar *str;
  gpointer param;
  gchar *expected_string;
  gboolean expected_result;
} ParserTestParam;

ParameterizedTestParameters(parser, test_string_parser)
{
  static ParserTestParam parser_params[] =
  {
    {.str = "foo", .param = NULL, .expected_string = "foo", .expected_result = TRUE},
    {.str = "foo bar", .param = NULL, .expected_string = "foo", .expected_result = TRUE},
    {.str = "foo123 bar", .param = NULL, .expected_string = "foo123", .expected_result = TRUE},
    {.str = "foo{}", .param = NULL, .expected_string = "foo", .expected_result = TRUE},
    {.str = "foo[]", .param = NULL, .expected_string = "foo", .expected_result = TRUE},
    {.str = "foo", .param = "X", .expected_string = "foo", .expected_result = TRUE},
    {.str = "foo=bar", .param = "=", .expected_string = "foo=bar", .expected_result = TRUE},
    {.str = "", .param = NULL, .expected_string = NULL, .expected_result = FALSE},
  };

  return cr_make_param_array(ParserTestParam, parser_params, G_N_ELEMENTS(parser_params));
}

ParameterizedTest(ParserTestParam *param, parser, test_string_parser)
{
  gchar *result_string = NULL;
  gboolean result;

  result = _invoke_parser(r_parser_string, param->str, param->param, NULL, &result_string);
  if (param->expected_result == TRUE)
    {
      cr_assert(result, "Mismatching parser result (true expected)");
      cr_assert_str_eq(result_string, param->expected_string, "Mismatching parser result (exp:%s, res:%s)",
                       param->expected_string, result_string);
      g_free(result_string);
    }
  else
    {
      cr_assert_not(result, "Mismatching parser result (false expected)");
    }
}

typedef struct _parser_test_qstring_param
{
  gchar *str;
  gchar *quotes;
  gchar *expected_string;
} ParserQStringTestParam;

ParameterizedTestParameters(parser, test_qstring_parser)
{
  static ParserQStringTestParam parser_params[] =
  {
    {.str = "'foo'", .quotes = "''", .expected_string = "foo"},
    {.str = "\"foo\"", .quotes = "\"\"", .expected_string = "foo"},
    {.str = "{foo}", .quotes = "{}", .expected_string = "foo"},
  };

  return cr_make_param_array(ParserQStringTestParam, parser_params, G_N_ELEMENTS(parser_params));
}

static gpointer
_compile_qstring_state(const gchar *quotes)
{
  union
  {
    gpointer ptr;
    gchar ending_char;
  } state;

  memset(&state, 0, sizeof(state));
  state.ending_char = quotes[1];
  return state.ptr;
}

ParameterizedTest(ParserQStringTestParam *param, parser, test_qstring_parser)
{
  gchar *result_string = NULL;
  gboolean result;

  result = _invoke_parser(r_parser_qstring, param->str, param->quotes, _compile_qstring_state(param->quotes),
                          &result_string);
  cr_assert(result, "Mismatching parser result");
  cr_assert_str_eq(result_string, param->expected_string, "Mismatching parser result (exp:%s, res:%s)",
                   param->expected_string,result_string);
  g_free(result_string);
}
