/*
 * Copyright (c) 2002-2013 Balabit
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
#include "cfg-lexer-subst.h"
#include "apphook.h"
#include <criterion/criterion.h>
#include <string.h>

static CfgArgs *
construct_cfg_args_for_args(const gchar *additional_values[])
{
  CfgArgs *args = cfg_args_new();
  gint i;

  cfg_args_set(args, "arg", "arg_value");
  cfg_args_set(args, "simple_string", "\"simple_string_value\"");
  cfg_args_set(args, "simple_qstring", "'simple_qstring_value'");
  cfg_args_set(args, "escaped_string", "\"escaped_string\\\"\\r\\n\"");

  cr_assert(cfg_args_contains(args, "simple-string"), "normalize: when set, must contain: simple-string");
  cr_assert(cfg_args_contains(args, "simple_string"), "normalize: when set, must contain: simple_string");

  for (i = 0; additional_values && additional_values[i] && additional_values[i + 1]; i += 2)
    {
      cfg_args_set(args, additional_values[i], additional_values[i + 1]);
    }
  return args;
}

static CfgArgs *
construct_cfg_args_for_defaults(void)
{
  CfgArgs *args = cfg_args_new();
  cfg_args_set(args, "arg", "default_for_arg");
  cfg_args_set(args, "def", "default_for_def");
  return args;
}

static CfgArgs *
construct_cfg_args_for_globals(void)
{
  CfgArgs *args = cfg_args_new();
  cfg_args_set(args, "arg", "global_for_arg");
  cfg_args_set(args, "def", "global_for_def");
  cfg_args_set(args, "globl", "global_for_globl");
  return args;
}

static CfgLexerSubst *
construct_object_with_values(const gchar *additional_values[])
{
  return cfg_lexer_subst_new(construct_cfg_args_for_globals(),
                             construct_cfg_args_for_defaults(),
                             construct_cfg_args_for_args(additional_values));
}

static CfgLexerSubst *
construct_object(void)
{
  return construct_object_with_values(NULL);
}

static gchar *
duplicate_string_without_with_trailing_nonzero_bytes(const gchar *input)
{
  gsize input_len = strlen(input);
  const gint canary_size = 10;
  gchar *input_dup = malloc(input_len + canary_size);

  memcpy(input_dup, input, input_len);
  memset(input_dup + input_len, 'A', canary_size - 1);
  input_dup[input_len + canary_size - 1] = 0;
  return input_dup;
}

static void
_assert_invoke_result(CfgLexerSubst *subst, gchar *input_dup, gssize input_len, const gchar *expected_output)
{
  gchar *result;
  gsize result_len;
  GError *error = NULL;

  result = cfg_lexer_subst_invoke(subst, input_dup, input_len, &result_len, &error);
  cr_assert_null(error, "Error value is non-null while no error is expected");
  cr_assert_not_null(result, "value substitution returned an unexpected failure");
  cr_assert_str_eq(result, expected_output, "value substitution is broken");
  cr_assert_eq(result_len, strlen(expected_output), "length returned by invoke_result is invalid");
  g_free(result);
}

static void
assert_invoke_result_with_input_length_explicit(CfgLexerSubst *subst, const gchar *input, const gchar *expected_output)
{
  gchar *input_dup;

  input_dup = duplicate_string_without_with_trailing_nonzero_bytes(input);
  _assert_invoke_result(subst, input_dup, strlen(input), expected_output);
  g_free(input_dup);
}

static void
assert_invoke_result_with_zero_terminated_input(CfgLexerSubst *subst, const gchar *input, const gchar *expected_output)
{
  gchar *input_dup;

  input_dup = strdup(input);
  _assert_invoke_result(subst, input_dup, -1, expected_output);
  g_free(input_dup);
}

static void
assert_invoke_result(CfgLexerSubst *subst, const gchar *input, const gchar *expected_output)
{
  assert_invoke_result_with_input_length_explicit(subst, input, expected_output);
  assert_invoke_result_with_zero_terminated_input(subst, input, expected_output);
}

static void
assert_invoke_failure(CfgLexerSubst *subst, const gchar *input, const gchar *expected_error)
{
  gchar *result;
  gsize result_len;
  gchar *input_dup = g_strdup(input);
  GError *error = NULL;

  result = cfg_lexer_subst_invoke(subst, input_dup, strlen(input), &result_len, &error);
  cr_assert_null(result, "expected failure for value substitution, but success was returned");
  cr_assert_not_null(error, "expected a non-NULL error object for failure");
  cr_assert_str_eq(error->message, expected_error, "error message mismatch");
  g_free(input_dup);
}

Test(cfg_lexer_subst, test_double_backtick_replaced_with_a_single_one)
{
  CfgLexerSubst *subst = construct_object();
  assert_invoke_result(subst, "``", "`");
  cfg_lexer_subst_free(subst);
}

Test(cfg_lexer_subst, test_single_backtick_causes_an_error)
{
  CfgLexerSubst *subst = construct_object();
  assert_invoke_failure(subst, "foo ` bar", "missing closing backtick (`) character");
  cfg_lexer_subst_free(subst);
}

Test(cfg_lexer_subst, test_backtick_after_quoted_character_succeeds)
{
  CfgLexerSubst *subst = construct_object();
  assert_invoke_result(subst, "foo \"string \\n`arg`\" bar", "foo \"string \\narg_value\" bar");
  cfg_lexer_subst_free(subst);
}

Test(cfg_lexer_subst, test_backtick_as_a_quoted_character_in_a_string_results_in_failure)
{
  CfgLexerSubst *subst = construct_object();

  assert_invoke_failure(subst, "foo \"string \\`arg`\" bar",
                        "cannot subsitute backticked values right after a string quote character");
  cfg_lexer_subst_free(subst);
}

Test(cfg_lexer_subst, test_value_in_normal_text_replaced_with_its_literal_value)
{
  CfgLexerSubst *subst = construct_object();
  assert_invoke_result(subst, "foo `arg` bar", "foo arg_value bar");
  assert_invoke_result(subst, "foo `simple_string` bar", "foo \"simple_string_value\" bar");
  assert_invoke_result(subst, "foo `simple_qstring` bar", "foo 'simple_qstring_value' bar");
  assert_invoke_result(subst, "foo `escaped_string` bar", "foo \"escaped_string\\\"\\r\\n\" bar");
  cfg_lexer_subst_free(subst);
}

Test(cfg_lexer_subst, test_values_are_resolution_order_args_defaults_globals_env)
{
  CfgLexerSubst *subst = construct_object();

  setenv("env", "env_for_env", TRUE);
  assert_invoke_result(subst, "foo `arg` bar", "foo arg_value bar");
  assert_invoke_result(subst, "foo `def` bar", "foo default_for_def bar");
  assert_invoke_result(subst, "foo `globl` bar", "foo global_for_globl bar");
  assert_invoke_result(subst, "foo `env` bar", "foo env_for_env bar");
  cfg_lexer_subst_free(subst);
}

Test(cfg_lexer_subst, test_values_are_inserted_within_strings)
{
  CfgLexerSubst *subst = construct_object();

  assert_invoke_result(subst, "foo \"`arg`\" bar", "foo \"arg_value\" bar");
  assert_invoke_result(subst, "foo '`arg`' bar", "foo 'arg_value' bar");
  cfg_lexer_subst_free(subst);
}

Test(cfg_lexer_subst, test_string_literals_are_inserted_into_strings_without_quotes)
{
  const gchar *additional_values[] =
  {
    "simple_string_with_whitespace", "  \"string_with_whitespace\"   ",
    NULL, NULL
  };
  CfgLexerSubst *subst = construct_object_with_values(additional_values);

  /* double quotes */
  assert_invoke_result(subst, "foo \"x `simple_string` y\" bar", "foo \"x simple_string_value y\" bar");
  /* apostrophes */
  assert_invoke_result(subst, "foo 'x `simple_string` y' bar", "foo 'x simple_string_value y' bar");
  assert_invoke_result(subst, "foo \"x `simple_string_with_whitespace` y\" bar",
                       "foo \"x string_with_whitespace y\" bar");
  cfg_lexer_subst_free(subst);
}

Test(cfg_lexer_subst, test_incorrect_strings_and_multiple_tokens_are_inserted_verbatim)
{
  const gchar *additional_values[] =
  {
    "half_string", "\"halfstring",
    "tokens_that_start_with_string", "\"str\", token",
    "tokens_enclosed_in_strings", "\"str1\", token, \"str2\"",
    NULL, NULL
  };
  CfgLexerSubst *subst = construct_object_with_values(additional_values);

  assert_invoke_result(subst, "foo \"x `simple_string` y\" bar", "foo \"x simple_string_value y\" bar");
  assert_invoke_result(subst, "foo \"x `half_string` y\" bar", "foo \"x \"halfstring y\" bar");
  assert_invoke_result(subst, "foo \"x `tokens_that_start_with_string` y\" bar", "foo \"x \"str\", token y\" bar");
  assert_invoke_result(subst, "foo \"x `tokens_enclosed_in_strings` y\" bar",
                       "foo \"x \"str1\", token, \"str2\" y\" bar");
  cfg_lexer_subst_free(subst);
}

Test(cfg_lexer_subst, test_strings_with_special_chars_are_properly_encoded_in_strings)
{
  const gchar *additional_values[] =
  {
    "string_with_characters_that_need_quoting", "\"quote: \\\", newline: \\r\\n, backslash: \\\\\"",
    NULL, NULL
  };
  CfgLexerSubst *subst = construct_object_with_values(additional_values);

  assert_invoke_result(subst, "foo \"x `string_with_characters_that_need_quoting` y\" bar",
                       "foo \"x quote: \\\", newline: \\r\\n, backslash: \\\\ y\" bar");
  cfg_lexer_subst_free(subst);
}

Test(cfg_lexer_subst, test_strings_with_embedded_apostrophe_cause_an_error_when_encoding_in_qstring)
{
  const gchar *additional_values[] =
  {
    "string_with_apostrophe", "\"'foo'\"",
    NULL, NULL
  };
  CfgLexerSubst *subst = construct_object_with_values(additional_values);

  assert_invoke_result(subst, "foo \"x `string_with_apostrophe` y\" bar", "foo \"x 'foo' y\" bar");
  assert_invoke_failure(subst, "foo 'x `string_with_apostrophe` y' bar",
                        "cannot represent apostrophes within apostroph-enclosed string");
  cfg_lexer_subst_free(subst);
}

Test(cfg_lexer_subst, test_tracking_string_state)
{
  const gchar *additional_values[] =
  {
    "quoted_escaped_newline", "\"\\n\"",
    NULL, NULL
  };

  CfgLexerSubst *subst = construct_object_with_values(additional_values);

  assert_invoke_result(subst, "\"hello\\n\" `quoted_escaped_newline`", "\"hello\\n\" \"\\n\"");
  assert_invoke_result(subst, "\"hello\\n\\n\" `quoted_escaped_newline`", "\"hello\\n\\n\" \"\\n\"");
  assert_invoke_result(subst, "\"hello\\n\\n \" `quoted_escaped_newline`", "\"hello\\n\\n \" \"\\n\"");
  cfg_lexer_subst_free(subst);
}

TestSuite(cfg_lexer_subst, .init = app_startup, .fini = app_shutdown);
