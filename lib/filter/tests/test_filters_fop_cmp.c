/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2013 Gergely Nagy <algernon@balabit.hu>
 * Copyright (c) 2021 Balazs Scheidler <bazsi77@gmail.com>
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

#include <criterion/criterion.h>
#include "libtest/cr_template.h"

#include "logmsg/logmsg.h"
#include "apphook.h"
#include "filter/filter-cmp.h"
#include "filter/filter-expr-grammar.h"

LogMessage *
_construct_sample_message(void)
{
  LogMessage *msg = log_msg_new_empty();

  msg->pri = 15;
  log_msg_set_value(msg, LM_V_PROGRAM, "software", -1);
  log_msg_set_value_by_name_with_type(msg, "strvalue", "string", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, "jsonvalue", "{\"foo\":\"foovalue\"}", -1, LM_VT_JSON);
  log_msg_set_value_by_name_with_type(msg, "truevalue", "1", -1, LM_VT_BOOLEAN);
  log_msg_set_value_by_name_with_type(msg, "falsevalue", "0", -1, LM_VT_BOOLEAN);
  log_msg_set_value_by_name_with_type(msg, "int32value", "32", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, "int64value", "4294967296", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, "nanvalue", "nan", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, "dblvalue", "3.1415", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, "datevalue", "1653246684.123", -1, LM_VT_DATETIME);
  log_msg_set_value_by_name_with_type(msg, "listvalue", "foo,bar,baz", -1, LM_VT_LIST);
  log_msg_set_value_by_name_with_type(msg, "nullvalue", "", -1, LM_VT_NULL);
  log_msg_set_value_by_name_with_type(msg, "bytesvalue", "\0\1\2\3", 4, LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, "protobufvalue", "\4\5\6\7", 4, LM_VT_PROTOBUF);
  return msg;
}

FilterExprNode *
_construct_filter(const gchar *config_snippet)
{
  FilterExprNode *tmp;

  CfgLexer *lexer = cfg_lexer_new_buffer(configuration, config_snippet, strlen(config_snippet));
  cr_assert(lexer);

  cr_assert(cfg_run_parser(configuration, lexer, &filter_expr_parser, (gpointer *) &tmp, NULL));
  cr_assert(tmp);
  return tmp;
}

gboolean
evaluate(const gchar *filter_expr)
{
  LogMessage *msg = _construct_sample_message();
  FilterExprNode *filter_node = _construct_filter(filter_expr);
  gboolean result;

  cr_assert_not_null(filter_node, "Error compiling filter expression");
  result = filter_expr_eval(filter_node, msg);

  log_msg_unref(msg);
  filter_expr_unref(filter_node);
  return result;
}

Test(filter, test_num_eq)
{
  cr_assert(evaluate("10 == 10"));
  cr_assert(evaluate("$SEVERITY_NUM == 7"));
  cr_assert(evaluate("$SEVERITY_NUM == $SEVERITY_NUM"));

  cr_assert_not(evaluate("10 == 11"));
}

Test(filter, test_num_ne)
{
  cr_assert(evaluate("10 != 9"));
  cr_assert(evaluate("$SEVERITY_NUM != 8"));
  cr_assert(evaluate("$SEVERITY_NUM != $FACILITY_NUM"));

  cr_assert_not(evaluate("10 != 10"));
}

Test(filter, test_num_lt)
{
  cr_assert(evaluate("10 < 11"));
  cr_assert(evaluate("7 < 8"));
  cr_assert(evaluate("7 < 10"));
  cr_assert(evaluate("$LEVEL_NUM < 8"));
  cr_assert(evaluate("$LEVEL_NUM < 10"));

  cr_assert_not(evaluate("11 < 10"));
  cr_assert_not(evaluate("11 < 11"));
}

Test(filter, test_num_le)
{
  cr_assert(evaluate("11 <= 11"));
  cr_assert(evaluate("10 <= 11"));
  cr_assert(evaluate("7 <= 8"));
  cr_assert(evaluate("7 <= 10"));
  cr_assert(evaluate("$LEVEL_NUM <= 8"));
  cr_assert(evaluate("$LEVEL_NUM <= 10"));

  cr_assert_not(evaluate("11 <= 10"));
}

Test(filter, test_num_gt)
{
  cr_assert(evaluate("11 > 10"));
  cr_assert(evaluate("8 > 7"));
  cr_assert(evaluate("10 > 7"));
  cr_assert(evaluate("8 > $LEVEL_NUM"));
  cr_assert(evaluate("10 > $LEVEL_NUM"));

  cr_assert_not(evaluate("10 > 11"));
  cr_assert_not(evaluate("10 > 10"));
}

Test(filter, test_num_ge)
{
  cr_assert(evaluate("10 >= 10"));
  cr_assert(evaluate("11 >= 10"));
  cr_assert(evaluate("8 >= 7"));
  cr_assert(evaluate("10 >= 7"));
  cr_assert(evaluate("8 >= $LEVEL_NUM"));
  cr_assert(evaluate("10 >= $LEVEL_NUM"));

  cr_assert_not(evaluate("10 >= 11"));
}

Test(filter, test_numeric_ordering)
{
  cr_assert_not(evaluate("10 < 10"));
  cr_assert(evaluate("10 <= 10"));
  cr_assert(evaluate("10 == 10"));
  cr_assert(evaluate("10 >= 10"));
  cr_assert_not(evaluate("10 > 10"));

  cr_assert(evaluate("10 < 11"));
  cr_assert(evaluate("10 <= 11"));
  cr_assert_not(evaluate("10 == 11"));
  cr_assert_not(evaluate("10 >= 11"));
  cr_assert_not(evaluate("10 > 11"));

  cr_assert_not(evaluate("11 < 10"));
  cr_assert_not(evaluate("11 <= 10"));
  cr_assert_not(evaluate("11 == 10"));
  cr_assert(evaluate("11 >= 10"));
  cr_assert(evaluate("11 > 10"));
}

/* string comparison */

Test(filter, test_eq)
{
  cr_assert(evaluate("10 eq 10"));
  cr_assert(evaluate("$SEVERITY_NUM eq 7"));
  cr_assert(evaluate("$SEVERITY_NUM eq $SEVERITY_NUM"));

  cr_assert_not(evaluate("10 eq 11"));

}

Test(filter, test_ne)
{
  cr_assert(evaluate("10 ne 9"));
  cr_assert(evaluate("$SEVERITY_NUM ne 8"));
  cr_assert(evaluate("$SEVERITY_NUM ne $FACILITY_NUM"));

  cr_assert_not(evaluate("10 ne 10"));
}

Test(filter, test_lt)
{
  cr_assert(evaluate("10 lt 11"));
  cr_assert(evaluate("7 lt 8"));
  cr_assert(evaluate("$LEVEL_NUM lt 8"));

  cr_assert_not(evaluate("7 lt 10"));
  cr_assert_not(evaluate("11 lt 10"));
  cr_assert_not(evaluate("11 lt 11"));
}

Test(filter, test_le)
{
  cr_assert(evaluate("11 le 11"));
  cr_assert(evaluate("10 le 11"));
  cr_assert(evaluate("7 le 8"));
  cr_assert(evaluate("$LEVEL_NUM le 8"));

  cr_assert_not(evaluate("7 le 10"));
  cr_assert_not(evaluate("11 le 10"));
  cr_assert_not(evaluate("$LEVEL_NUM le 10"));
}

Test(filter, test_gt)
{
  cr_assert(evaluate("11 gt 10"));
  cr_assert(evaluate("8 gt 7"));
  cr_assert(evaluate("8 gt $LEVEL_NUM"));

  cr_assert_not(evaluate("10 gt 7"));
  cr_assert_not(evaluate("10 gt 11"));
  cr_assert_not(evaluate("10 gt 10"));
  cr_assert_not(evaluate("10 gt $LEVEL_NUM"));
}

Test(filter, test_ge)
{
  cr_assert(evaluate("10 ge 10"));
  cr_assert(evaluate("11 ge 10"));
  cr_assert(evaluate("8 ge 7"));
  cr_assert(evaluate("8 ge $LEVEL_NUM"));

  cr_assert_not(evaluate("10 ge 7"));
  cr_assert_not(evaluate("10 ge 11"));
  cr_assert_not(evaluate("10 ge $LEVEL_NUM"));
}

Test(filter, test_string_ordering)
{
  cr_assert_not(evaluate("10 lt 10"));
  cr_assert(evaluate("10 le 10"));
  cr_assert(evaluate("10 eq 10"));
  cr_assert(evaluate("10 ge 10"));
  cr_assert_not(evaluate("10 gt 10"));

  cr_assert(evaluate("10 lt 11"));
  cr_assert(evaluate("10 le 11"));
  cr_assert_not(evaluate("10 eq 11"));
  cr_assert_not(evaluate("10 ge 11"));
  cr_assert_not(evaluate("10 gt 11"));

  cr_assert_not(evaluate("11 lt 10"));
  cr_assert_not(evaluate("11 le 10"));
  cr_assert_not(evaluate("11 eq 10"));
  cr_assert(evaluate("11 ge 10"));
  cr_assert(evaluate("11 gt 10"));
}

Test(filter, test_string_ordering_with_non_numbers)
{
  cr_assert(evaluate("alma lt korte"));
  cr_assert(evaluate("alma le korte"));
  cr_assert_not(evaluate("alma eq korte"));
  cr_assert_not(evaluate("alma ge korte"));
  cr_assert_not(evaluate("alma gt korte"));

  cr_assert_not(evaluate("korte lt alma"));
  cr_assert_not(evaluate("korte le alma"));
  cr_assert_not(evaluate("korte eq alma"));
  cr_assert(evaluate("korte ge alma"));
  cr_assert(evaluate("korte gt alma"));
}

Test(filter, test_type_aware_comparisons_strings_to_strings_are_compared_as_strings)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  /* strings */
  cr_assert(evaluate("'alma' != 'korte'"));
  cr_assert_not(evaluate("'alma' == 'korte'"));
  cr_assert(evaluate("'alma' < 'korte'"));
  cr_assert(evaluate("'korte' > 'alma'"));

  /* strings containing numbers */
  cr_assert(evaluate("'10' != '11'"));
  cr_assert_not(evaluate("'10' == '11'"));
  cr_assert(evaluate("'10' < '7'"));
  cr_assert(evaluate("'7' > '10'"));

  /* string values */
  cr_assert(evaluate("'$strvalue' == 'string'"));
  cr_assert(evaluate("'$strvalue' == '$strvalue'"));
  cr_assert_not(evaluate("'$strvalue' != '$strvalue'"));

  /* bytes values */
  cr_assert(evaluate("'$bytesvalue' == '$bytesvalue'"));
  cr_assert_not(evaluate("'$bytesvalue' != '$bytesvalue'"));
  cr_assert(evaluate("'$protobufvalue' == '$protobufvalue'"));
  cr_assert_not(evaluate("'$protobufvalue' != '$protobufvalue'"));
}

Test(filter, test_type_aware_comparison_objects_are_compared_as_strings_if_types_match)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  cr_assert(evaluate("'$jsonvalue' == json('{\"foo\":\"foovalue\"}')"));
  cr_assert(evaluate("'$listvalue' == list('foo,bar,baz')"));

  /* types are not equal, they'd be compared as numbers, both are NaNs */
  cr_assert_not(evaluate("list('foo,bar,baz') == string('foo,bar,baz')"));
  cr_assert_not(evaluate("list('') == string('')"));
}

Test(filter, test_type_aware_comparison_strings_are_compared_as_strings_if_types_match)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  cr_assert(evaluate("'$strvalue' == 'string')"));
  cr_assert(evaluate("'$strvalue' > 'foo')"));
  cr_assert(evaluate("'$strvalue' < 'zabkasa')"));
}

Test(filter, test_type_aware_comparison_null_equals_to_null_and_does_not_equal_to_anything_else)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  cr_assert(evaluate("null('') == null('')"));
  cr_assert_not(evaluate("null('') != null('')"));
  cr_assert(evaluate("'$nullvalue' == null('')"));
  cr_assert_not(evaluate("'$nullvalue' != null('')"));
  cr_assert(evaluate("string('') != null('')"));
  cr_assert(evaluate("int64('0') != null('')"));
  cr_assert(evaluate("double('0.0') != null('')"));
  cr_assert(evaluate("list('') != null('')"));
  cr_assert(evaluate("json('') != null('')"));
  cr_assert(evaluate("datetime('') != null('')"));
}


Test(filter, test_non_existing_macro_has_a_null_value_so_it_equals_to_null)
{
  /* not existing macro is null */
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  cr_assert(evaluate("'$doesnotexist' == null('')"));
  cr_assert_not(evaluate("'$doesnotexist' != null('')"));
}

Test(filter, test_type_aware_comparison_null_converts_to_zero_when_compared_with_less_than_or_greater_than)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  cr_assert(evaluate("'$nullvalue' < 1"));
  cr_assert(evaluate("'$nullvalue' > -1"));
}

Test(filter, test_compat_mode_numeric_comparisons_are_compared_as_numbers)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_3_36);

  /* strings are converted to "0" before comparison */
  cr_assert(evaluate("'alma' == 'korte'"));
  cr_assert_not(evaluate("'alma' != 'korte'"));
  cr_assert_not(evaluate("'alma' < 'korte'"));
  cr_assert_not(evaluate("'korte' > 'alma'"));

  cr_assert(evaluate("'$strvalue' == 'string'"));
  cr_assert(evaluate("'$strvalue' == '$strvalue'"));
  cr_assert_not(evaluate("'$strvalue' != '$strvalue'"));

  /* strings containing numbers are converted to numbers and compared numerically */
  cr_assert(evaluate("'10' != '11'"));
  cr_assert_not(evaluate("'10' == '11'"));
  cr_assert(evaluate("'10' > '7'"));
  cr_assert(evaluate("'7' < '10'"));

}

Test(filter, test_type_aware_comparisons_mixed_types_or_numbers_are_compared_as_numbers_after_conversion)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  cr_assert(evaluate("int('$int32value') == '32'"));
  cr_assert(evaluate("int('$int32value') < '321'"));
  cr_assert(evaluate("int('$int32value') > '7'"));

  /* booleans are converted to 0/1 */
  cr_assert(evaluate("'$truevalue' == 1"));
  cr_assert(evaluate("'$falsevalue' == 0"));

  cr_assert(evaluate("'$dblvalue' < 3.145"));
  cr_assert(evaluate("'$dblvalue' > 3.14"));
  cr_assert(evaluate("'$dblvalue' > 0.314e1"));
  cr_assert(evaluate("'$dblvalue' < 0.314e2"));
  cr_assert(evaluate("'$datevalue' == 1653246684123"));
  cr_assert(evaluate("'$nullvalue' < 1"));
  cr_assert(evaluate("'$nullvalue' > -1"));

  /* JSON objects/lists are always NaN values, which produce FALSE comparisons */
  cr_assert_not(evaluate("'$listvalue' < 01234"));
  cr_assert_not(evaluate("'$listvalue' > 01234"));
  cr_assert_not(evaluate("'$listvalue' == 01234"));
  cr_assert_not(evaluate("'$jsonvalue' < 01234"));
  cr_assert_not(evaluate("'$jsonvalue' > 01234"));
  cr_assert_not(evaluate("'$jsonvalue' == 01234"));

  /* bytes and protobuf types are behaving as they were unset (return NULL values) if not explicitly cast to bytes() */
  cr_assert(evaluate("'$bytesvalue' < 1"));
  cr_assert(evaluate("'$bytesvalue' > -1"));
  cr_assert(evaluate("'$protobufvalue' < 1"));
  cr_assert(evaluate("'$protobufvalue' > -1"));

  /* bytes and protobuf types are NaN values if explicitly cast to bytes() */
  cr_assert_not(evaluate("bytes('$bytesvalue') < 01234"));
  cr_assert_not(evaluate("bytes('$bytesvalue') > 01234"));
  cr_assert_not(evaluate("bytes('$bytesvalue') == 01234"));
  cr_assert_not(evaluate("protobuf('$protobufvalue') < 01234"));
  cr_assert_not(evaluate("protobuf('$protobufvalue') > 01234"));
  cr_assert_not(evaluate("protobuf('$protobufvalue') == 01234"));
}

Test(filter, test_type_aware_comparison_nan_is_always_different_from_anything)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  cr_assert(evaluate("'$nanvalue' != '$nanvalue'"));
  cr_assert_not(evaluate("'$nanvalue' < '5'"));
  cr_assert_not(evaluate("'$nanvalue' > '5'"));
  cr_assert_not(evaluate("'$nanvalue' == '5'"));
  cr_assert(evaluate("'5' != '$nanvalue'"));
  cr_assert_not(evaluate("'$nanvalue' == '$nanvalue'"));
  cr_assert_not(evaluate("'$nanvalue' == int64('nan')"));
  cr_assert_not(evaluate("'$nanvalue' == int64('foobar')"));
  cr_assert_not(evaluate("'$nanvalue' < '$nanvalue'"));
  cr_assert_not(evaluate("'$nanvalue' > '$nanvalue'"));
}

Test(filter, test_type_and_value_comparison_checks_whether_type_and_value_match_completely)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  cr_assert(evaluate("$strvalue === $strvalue"));
  cr_assert(evaluate("$strvalue === string"));
  cr_assert(evaluate("string(64) === string(64)"));
  cr_assert_not(evaluate("string(64) !== string(64)"));
  cr_assert_not(evaluate("string(64) === int64(64)"));
  cr_assert(evaluate("string(64) !== int64(64)"));

  /* types match, values don't */
  cr_assert_not(evaluate("foo === bar"));
  cr_assert_not(evaluate("int64(123) === int64(256)"));
  cr_assert_not(evaluate("123 === 456"));

  /* string conversion */
  cr_assert(evaluate("double(  1e1  ) === double(10)"));
}

Test(filter, test_int32_and_int64_are_aliases_to_int)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  cr_assert(evaluate("int32(64) === int64(64)"));
  cr_assert(evaluate("int32(64) === int(64)"));
  cr_assert(evaluate("int64(64) === int(64)"));
}

static void
setup(void)
{
  app_startup();

  configuration = cfg_new_snippet();
}

static void
teardown(void)
{
  app_shutdown();
  cfg_free(configuration);
}

TestSuite(filter, .init = setup, .fini = teardown);
