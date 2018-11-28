/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2013 Gergely Nagy <algernon@balabit.hu>
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
#include "filter/filter-expr.h"
#include "filter/filter-re.h"
#include "filter/filter-pri.h"
#include "filter/filter-op.h"
#include "cfg.h"
#include "test_filters_common.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

TestSuite(filter, .init = setup, .fini = teardown);

typedef struct _FilterParamRegexp
{
  const gchar *msg;
  gint field;
  const gchar *regexp;
  gint flags;
  const gchar *regexp2;
  gint flags2;
  gboolean expected_result;
  const gchar *name;
  const gchar *value;
} FilterParamRegexp;

Test(filter, create_pcre_regexp_filter)
{
  cr_assert_eq(create_pcre_regexp_filter(LM_V_PROGRAM, "((", 0), NULL);
  cr_assert_eq(create_pcre_regexp_filter(LM_V_HOST, "((", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match("((", 0), NULL);
  cr_assert_eq(create_pcre_regexp_filter(LM_V_PROGRAM, "((", 0), NULL);
  cr_assert_eq(create_pcre_regexp_filter(LM_V_HOST, "((", 0), NULL);
  cr_assert_eq(create_pcre_regexp_filter(LM_V_HOST, "(?iana", 0), NULL);
  cr_assert_eq(create_pcre_regexp_filter(LM_V_HOST, "(?iana", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match("((", 0), NULL);
}

ParameterizedTestParameters(filter, test_filter_regexp_backref_chk)
{
  static FilterParamRegexp test_data_list[] =
  {
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: al fa", .field = LM_V_MESSAGE, .regexp = "(a)(l) (fa)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "a"},
    {.msg = "<15>Oct 15 16:17:02 host openvpn[2499]: al fa", .field = LM_V_MESSAGE, .regexp = "(a)(l) (fa)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "0", .value = "al fa"},
    {.msg = "<15>Oct 15 16:17:03 host openvpn[2499]: al fa", .field = LM_V_MESSAGE, .regexp = "(a)(l) (fa)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "232", .value = NULL},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: alma fa", .field = LM_V_MESSAGE, .regexp = "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa>fa)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "MM", .value = "m"},
    {.msg = "<15>Oct 15 16:17:02 host openvpn[2499]: alma fa", .field = LM_V_MESSAGE, .regexp = "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa>fa)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "aaaa", .value = NULL},
    {.msg = "<15>Oct 15 16:17:03 host openvpn[2499]: alma fa", .field = LM_V_MESSAGE, .regexp = "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa_name>fa)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "fa_name", .value = "fa"},
    {.msg = "<15>Oct 15 16:17:04 host openvpn[2499]: al fa", .field = LM_V_MESSAGE, .regexp = "(a)(l) (fa)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "2", .value = "l"},
    {.msg = "<15>Oct 15 16:17:05 host openvpn[2499]: al fa", .field = LM_V_MESSAGE, .regexp = "(a)(l) (fa)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "0", .value = "al fa"},
    {.msg = "<15>Oct 15 16:17:06 host openvpn[2499]: al fa", .field = LM_V_MESSAGE, .regexp = "(a)(l) (fa)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "233", .value = NULL}
  };

  return cr_make_param_array(FilterParamRegexp, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(FilterParamRegexp *param, filter, test_filter_regexp_backref_chk)
{
  FilterExprNode *filter = create_pcre_regexp_filter(param->field, param->regexp, param->flags);
  testcase_with_backref_chk(param->msg, filter, param->expected_result, param->name, param->value);
}

ParameterizedTestParameters(filter, test_filter_regexp_filter)
{
  static FilterParamRegexp test_data_list[] =
  {
    {.msg = "<15> openvpn[2501]: PTHREAD support initialized", .field = LM_V_PROGRAM, .regexp = "^openvpn$", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2500]: PTHREAD support initialized", .field = LM_V_PROGRAM, .regexp = "^open$", .flags = 0, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .field = LM_V_HOST, .regexp = "^host$", .flags = 0, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:02 host openvpn[2499]: PTHREAD support initialized", .field = LM_V_HOST, .regexp = "^hos$", .flags = 0, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:03 host openvpn[2499]: PTHREAD support initialized", .field = LM_V_HOST, .regexp = "pthread", .flags = 0, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:04 host openvpn[2499]: PTHREAD support initialized", .field = LM_V_MESSAGE, .regexp = "^PTHREAD ", .flags = 0, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:05 host openvpn[2499]: PTHREAD support initialized", .field = LM_V_MESSAGE, .regexp = "PTHREAD s", .flags = 0, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:06 host openvpn[2499]: PTHREAD support initialized", .field = LM_V_MESSAGE, .regexp = "^PTHREAD$", .flags = 0, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:07 host openvpn[2499]: PTHREAD support initialized", .field = LM_V_MESSAGE, .regexp = "(?i)pthread", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2499]: PTHREAD support initialized", .field = LM_V_PROGRAM, .regexp = "^openvpn$", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2498]: PTHREAD support initialized", .field = LM_V_PROGRAM, .regexp = "^open$", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2497]: PTHREAD support initialized", .field = LM_V_HOST, .regexp = "^host$", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2496]: PTHREAD support initialized", .field = LM_V_HOST, .regexp = "^hos$", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2495]: PTHREAD support initialized", .field = LM_V_HOST, .regexp = "pthread", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2494]: PTHREAD support initialized", .field = LM_V_MESSAGE, .regexp = "^PTHREAD ", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2493]: PTHREAD support initialized", .field = LM_V_MESSAGE, .regexp = "PTHREAD s", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2492]: PTHREAD support initialized", .field = LM_V_MESSAGE, .regexp = "^PTHREAD$", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: PTHREAD support initialized", .field = LM_V_MESSAGE, .regexp = "(?i)pthread", .flags = 0, .expected_result = TRUE},
  };

  return cr_make_param_array(FilterParamRegexp, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(FilterParamRegexp *param, filter, test_filter_regexp_filter)
{
  FilterExprNode *filter = create_pcre_regexp_filter(param->field, param->regexp, param->flags);
  testcase(param->msg, filter, param->expected_result);
}

ParameterizedTestParameters(filter, test_filter_regexp_filter_fop)
{
  static FilterParamRegexp test_data_list[] =
  {
    {.msg = "<15>Oct 16 16:17:01 host openvpn[2499]: PTHREAD support initialized", .field = LM_V_PROGRAM, .regexp = " PTHREAD ", .flags = 0, .regexp2 = "PTHREAD", .flags2 = 0, .expected_result = TRUE},
    {.msg = "<15>Oct 16 16:17:02 host openvpn[2499]: PTHREAD support initialized", .field = LM_V_PROGRAM, .regexp = " PTHREAD ", .flags = 0, .regexp2 = "^PTHREAD$", .flags2 = 0, .expected_result = FALSE},
    {.msg = "<15>Oct 16 16:17:03 host openvpn[2499]: PTHREAD support initialized", .field = LM_V_PROGRAM, .regexp = "^PTHREAD$", .flags = 0, .regexp2 = " PTHREAD ", .flags2 = 0, .expected_result = FALSE},
    {.msg = "<15>Oct 16 16:17:04 host openvpn[2499]: PTHREAD support initialized", .field = LM_V_PROGRAM, .regexp = " PAD ", .flags = 0, .regexp2 = "^PTHREAD$", .flags2 = 0, .expected_result = FALSE},
  };

  return cr_make_param_array(FilterParamRegexp, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(FilterParamRegexp *param, filter, test_filter_regexp_filter_fop)
{
  FilterExprNode *filter = fop_and_new(create_pcre_regexp_match(param->regexp, param->flags),
                                       create_pcre_regexp_match(param->regexp2, param->flags2));
  testcase(param->msg, filter, param->expected_result);
}

ParameterizedTestParameters(filter, test_filter_regexp_match_fop)
{
  static FilterParamRegexp test_data_list[] =
  {
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .regexp = " PTHREAD ", .flags = 0, .regexp2 = "PTHREAD", .flags2 = 0, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:02 host openvpn[2499]: PTHREAD support initialized", .regexp = " PTHREAD ", .flags = 0, .regexp2 = "^PTHREAD$", .flags2 = 0, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:03 host openvpn[2499]: PTHREAD support initialized", .regexp = "^PTHREAD$", .flags = 0, .regexp2 = " PTHREAD ", .flags2 = 0, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:04 host openvpn[2499]: PTHREAD support initialized", .regexp = " PAD ", .flags = 0, .regexp2 = "^PTHREAD$", .flags2 = 0, .expected_result = FALSE},
  };
  return cr_make_param_array(FilterParamRegexp, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(FilterParamRegexp *param, filter, test_filter_regexp_match_fop)
{
  FilterExprNode *filter = fop_or_new(create_pcre_regexp_match(param->regexp, param->flags),
                                      create_pcre_regexp_match(param->regexp2, param->flags2));
  testcase(param->msg, filter, param->expected_result);
}

ParameterizedTestParameters(filter, test_filter_regexp_match)
{
  static FilterParamRegexp test_data_list[] =
  {
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .regexp = " PTHREAD ", .flags = 0, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .regexp = "^openvpn\\[2499\\]: PTHREAD", .flags = 0, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .regexp = "^PTHREAD$", .flags = 0, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .regexp = "(?i)pthread", .flags = 0, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .regexp = "pthread", .flags = LMF_ICASE, .expected_result = TRUE},
  };
  return cr_make_param_array(FilterParamRegexp, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(FilterParamRegexp *param, filter, test_filter_regexp_match)
{
  FilterExprNode *filter = create_pcre_regexp_match(param->regexp, param->flags);
  testcase(param->msg, filter, param->expected_result);
}
