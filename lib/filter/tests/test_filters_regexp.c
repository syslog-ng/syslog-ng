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
#include "libtest/cr_template.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pcre.h>

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

static gboolean
check_pcre_version_is_atleast(const gchar *version)
{
  return strncmp(pcre_version(), version, strlen(version)) >= 0;
}

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
  cr_assert_eq(create_pcre_regexp_match("(?P<foo_123", 0), NULL);  // Unterminated group identifier
  if (check_pcre_version_is_atleast("8.34"))
    cr_assert_eq(create_pcre_regexp_match("(?P<1>a)", 0), NULL);  // Begins with a digit
  cr_assert_eq(create_pcre_regexp_match("(?P<!>a)", 0), NULL);  // Begins with an illegal char
  cr_assert_eq(create_pcre_regexp_match("(?P<foo!>a)", 0), NULL);  // Ends with an illegal char
  cr_assert_eq(create_pcre_regexp_match("\\1", 0), NULL);  // Backreference
  cr_assert_eq(create_pcre_regexp_match("a[b-a]", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match("a[]b", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match("a[", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match("*a", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match("(*)b", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match("a\\", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match("abc)", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match("(abc", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match("a**", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match(")(", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match("(?<DN>foo)|(?<DN>bar)", 0), NULL);
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
    {.msg = "<15>Oct 15 16:17:04 host openvpn[2499]: al fa", .field = LM_V_MESSAGE, .regexp = "(a)(l) (fa)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "3", .value = "fa"},
    {.msg = "<15>Oct 15 16:17:05 host openvpn[2499]: al fa", .field = LM_V_MESSAGE, .regexp = "(a)(l) (fa)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "0", .value = "al fa"},
    {.msg = "<15>Oct 15 16:17:06 host openvpn[2499]: al fa", .field = LM_V_MESSAGE, .regexp = "(a)(l) (fa)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "233", .value = NULL},
    {.msg = "<15>Oct 15 16:17:06 host openvpn[2499]: foobar bar", .field = LM_V_MESSAGE, .regexp = "(?<foobar>foobar) (?<foo>foo)?(?<bar>bar)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "bar", .value = "bar"},
    {.msg = "<15>Oct 15 16:17:06 host openvpn[2499]: foobar bar", .field = LM_V_MESSAGE, .regexp = "(?<foobar>foobar) (?<foo>foo)?(?<bar>bar)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "foobar", .value = "foobar"},
    {.msg = "<15>Oct 15 16:17:06 host openvpn[2499]: foobar bar", .field = LM_V_MESSAGE, .regexp = "(?<foobar>foobar) (?<foo>foo)?(?<bar>bar)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "foo", .value = NULL},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abc", .field = LM_V_MESSAGE, .regexp = "((a))", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "a"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abc", .field = LM_V_MESSAGE, .regexp = "((a))", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "2", .value = "a"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: ab", .field = LM_V_MESSAGE, .regexp = "(a+|b)*", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "b"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: ab", .field = LM_V_MESSAGE, .regexp = "(a+|b){0,}", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "b"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: ab", .field = LM_V_MESSAGE, .regexp = "(a+|b)+", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "b"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: ab", .field = LM_V_MESSAGE, .regexp = "(a+|b){1,}", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "b"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: ab", .field = LM_V_MESSAGE, .regexp = "(a+|b)?", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "a"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: ab", .field = LM_V_MESSAGE, .regexp = "(a+|b){0,1}", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "a"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abbbcd", .field = LM_V_MESSAGE, .regexp = "([abc])*d", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "c"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abcd", .field = LM_V_MESSAGE, .regexp = "([abc])*bcd", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "a"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: xabyabbbz", .field = LM_V_MESSAGE, .regexp = "ab*", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "0", .value = "ab"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: xayabbbz", .field = LM_V_MESSAGE, .regexp = "ab*", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "0", .value = "a"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abcdef", .field = LM_V_MESSAGE, .regexp = "(abc|)ef", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "0", .value = "ef"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abcd", .field = LM_V_MESSAGE, .regexp = "(a|b)c*d", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "b"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abc", .field = LM_V_MESSAGE, .regexp = "(ab|ab*)bc", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "a"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abc", .field = LM_V_MESSAGE, .regexp = "a([bc]*)c*", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "bc"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abcd", .field = LM_V_MESSAGE, .regexp = "a([bc]*)(c*d)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "2", .value = "d"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abcd", .field = LM_V_MESSAGE, .regexp = "a([bc]+)(c*d)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "2", .value = "d"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abcd", .field = LM_V_MESSAGE, .regexp = "a([bc]*)(c+d)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "2", .value = "cd"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: alpha", .field = LM_V_MESSAGE, .regexp = "[a-zA-Z_][a-zA-Z0-9_]*", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "0", .value = "alpha"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abh", .field = LM_V_MESSAGE, .regexp = "^a(bc+|b[eh])g|.h$", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = NULL},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: effgz", .field = LM_V_MESSAGE, .regexp = "(bc+d$|ef*g.|h?i(j|k))", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "effgz"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: effgz", .field = LM_V_MESSAGE, .regexp = "(bc+d$|ef*g.|h?i(j|k))", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "2", .value = NULL},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: ij", .field = LM_V_MESSAGE, .regexp = "(bc+d$|ef*g.|h?i(j|k))", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "ij"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: ij", .field = LM_V_MESSAGE, .regexp = "(bc+d$|ef*g.|h?i(j|k))", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "2", .value = "j"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: reffgz", .field = LM_V_MESSAGE, .regexp = "(bc+d$|ef*g.|h?i(j|k))", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "effgz"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: reffgz", .field = LM_V_MESSAGE, .regexp = "(bc+d$|ef*g.|h?i(j|k))", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "2", .value = NULL},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: a", .field = LM_V_MESSAGE, .regexp = "((((((((((a))))))))))", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "10", .value = "a"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: aa", .field = LM_V_MESSAGE, .regexp = "((((((((((a))))))))))\\10", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "0", .value = "aa"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abcde", .field = LM_V_MESSAGE, .regexp = "(.*)c(.*)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "ab"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abcde", .field = LM_V_MESSAGE, .regexp = "(.*)c(.*)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "2", .value = "de"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: (a, b)", .field = LM_V_MESSAGE, .regexp = "\\((.*), (.*)\\)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "a"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: (a, b)", .field = LM_V_MESSAGE, .regexp = "\\((.*), (.*)\\)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "2", .value = "b"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abcabc", .field = LM_V_MESSAGE, .regexp = "(abc)\\1", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "abc"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: abcabc", .field = LM_V_MESSAGE, .regexp = "([a-c]*)\\1", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "abc"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: a:bc-:de:f", .field = LM_V_MESSAGE, .regexp = "(?<!-):(.*?)(?<!-):", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "bc-:de"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: A", .field = LM_V_MESSAGE, .regexp = "(?i)(?:(?:(?:(?:(?:(?:(?:(?:(?:(a))))))))))", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "A"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: C", .field = LM_V_MESSAGE, .regexp = "(?i)(?:(?:(?:(?:(?:(?:(?:(?:(?:(a|b|c))))))))))", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "C"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: ace", .field = LM_V_MESSAGE, .regexp = "a(?:b|c|d)(.)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "e"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: ace", .field = LM_V_MESSAGE, .regexp = "a(?:b|c|d)*(.)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "e"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: ace", .field = LM_V_MESSAGE, .regexp = "a(?:b|c|d)+?(.)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "e"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: ace", .field = LM_V_MESSAGE, .regexp = "a(?:b|(c|e){1,2}?|d)+?(.)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "1", .value = "c"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: ace", .field = LM_V_MESSAGE, .regexp = "a(?:b|(c|e){1,2}?|d)+?(.)", .flags = LMF_STORE_MATCHES, .expected_result = TRUE, .name = "2", .value = "e"},
    // using duplicate names for named subpatterns.
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: hello foo", .field = LM_V_MESSAGE, .regexp = "(?<DN>foo)|(?<DN>bar)", .flags = LMF_STORE_MATCHES | LMF_DUPNAMES, .expected_result = TRUE, .name = "DN", .value = "foo"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: hello bar", .field = LM_V_MESSAGE, .regexp = "(?<DN>foo)|(?<DN>bar)", .flags = LMF_STORE_MATCHES | LMF_DUPNAMES, .expected_result = TRUE, .name = "DN", .value = "bar"},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: foobar", .field = LM_V_MESSAGE, .regexp = "(?<DN>foo)(?<DN>bar)", .flags = LMF_STORE_MATCHES | LMF_DUPNAMES, .expected_result = TRUE, .name = "DN", .value = "bar"},
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
    {.msg = "<15> openvpn[2491]: a", .field = LM_V_MESSAGE, .regexp = "\\141", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: \1", .field = LM_V_MESSAGE, .regexp = "[\\1]", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abc", .field = LM_V_MESSAGE, .regexp = "ab*c", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abc", .field = LM_V_MESSAGE, .regexp = "ab*bc", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abbbbc", .field = LM_V_MESSAGE, .regexp = "ab{0,}bc", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abbc", .field = LM_V_MESSAGE, .regexp = "ab+bc", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abq", .field = LM_V_MESSAGE, .regexp = "ab+bc", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: abq", .field = LM_V_MESSAGE, .regexp = "ab+bc", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: abbbbc", .field = LM_V_MESSAGE, .regexp = "ab{1,3}bc", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abbbbc", .field = LM_V_MESSAGE, .regexp = "ab{4,5}bc", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: abbc", .field = LM_V_MESSAGE, .regexp = "ab?bc", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abbbbc", .field = LM_V_MESSAGE, .regexp = "ab?bc", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: axyzc", .field = LM_V_MESSAGE, .regexp = "a.*c", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: axyzd", .field = LM_V_MESSAGE, .regexp = "a.*c", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: abc", .field = LM_V_MESSAGE, .regexp = "a[bc]d", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: abd", .field = LM_V_MESSAGE, .regexp = "a[bc]d", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abd", .field = LM_V_MESSAGE, .regexp = "a[b-d]e", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: ace", .field = LM_V_MESSAGE, .regexp = "a[b-d]e", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: a-", .field = LM_V_MESSAGE, .regexp = "a[-b]", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: a-", .field = LM_V_MESSAGE, .regexp = "a[b-]", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: a]", .field = LM_V_MESSAGE, .regexp = "a]", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: a]b", .field = LM_V_MESSAGE, .regexp = "a[]]b", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: aed", .field = LM_V_MESSAGE, .regexp = "a[^bc]d", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abd", .field = LM_V_MESSAGE, .regexp = "a[^bc]d", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: adc", .field = LM_V_MESSAGE, .regexp = "a[^-b]c", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: a-c", .field = LM_V_MESSAGE, .regexp = "a[^-b]c", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: a]c", .field = LM_V_MESSAGE, .regexp = "a[^]b]", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: adc", .field = LM_V_MESSAGE, .regexp = "a[^]b]c", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abc", .field = LM_V_MESSAGE, .regexp = "ab|cd", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abcd", .field = LM_V_MESSAGE, .regexp = "ab|cd", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: a(b", .field = LM_V_MESSAGE, .regexp = "a\\(b", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: ab", .field = LM_V_MESSAGE, .regexp = "a\\(*b", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: a((b", .field = LM_V_MESSAGE, .regexp = "a\\(*b", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: a\\b", .field = LM_V_MESSAGE, .regexp = "a\\\\b", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abcabc", .field = LM_V_MESSAGE, .regexp = "a.+?c", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: effg", .field = LM_V_MESSAGE, .regexp = "(bc+d$|ef*g.|h?i(j|k))", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: bcdd", .field = LM_V_MESSAGE, .regexp = "(bc+d$|ef*g.|h?i(j|k))", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: abad", .field = LM_V_MESSAGE, .regexp = "a(?!b).", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abad", .field = LM_V_MESSAGE, .regexp = "a(?=d).", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: abad", .field = LM_V_MESSAGE, .regexp = "a(?=c|d).", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: a\nb", .field = LM_V_MESSAGE, .regexp = "a.b", .flags = 0, .expected_result = FALSE},
    {.msg = "<15> openvpn[2491]: a\nb", .field = LM_V_MESSAGE, .regexp = "(?s)a.b", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: --ab_cd0123--", .field = LM_V_MESSAGE, .regexp = "\\w+", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: --ab_cd0123--", .field = LM_V_MESSAGE, .regexp = "[\\w]+", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: 1234abc5678", .field = LM_V_MESSAGE, .regexp = "\\D+", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: 1234abc5678", .field = LM_V_MESSAGE, .regexp = "[\\D]+", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: 123abc", .field = LM_V_MESSAGE, .regexp = "[\\da-fA-F]+", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]:  testing!1972", .field = LM_V_MESSAGE, .regexp = "([\\s]*)([\\S]*)([\\s]*)", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]:  testing!1972", .field = LM_V_MESSAGE, .regexp = "(\\s*)(\\S*)(\\s*)", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: \377", .field = LM_V_MESSAGE, .regexp = "\\xff", .flags = 0, .expected_result = TRUE},
    {.msg = "<15> openvpn[2491]: \377", .field = LM_V_MESSAGE, .regexp = "\\x00ff", .flags = 0, .expected_result = FALSE},
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

Test(filter, test_match_with_value)
{
  FilterExprNode *filter;

  filter = create_pcre_regexp_match("^PTHREAD", 0);
  filter_match_set_value_handle(filter, LM_V_MESSAGE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter, TRUE);

  filter = create_pcre_regexp_match("^2499", 0);
  filter_match_set_value_handle(filter, LM_V_PID);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter, TRUE);
}

Test(filter, test_match_with_template)
{
  FilterExprNode *filter;

  filter = create_pcre_regexp_match("^PTHREAD", 0);
  filter_match_set_template_ref(filter, compile_template("$MSG", FALSE));
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter, TRUE);

  filter = create_pcre_regexp_match("^2499", 0);
  filter_match_set_template_ref(filter, compile_template("$PID", FALSE));
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter, TRUE);

  filter = create_pcre_regexp_match("^2499 openvpn", 0);
  filter_match_set_template_ref(filter, compile_template("$PID $PROGRAM", FALSE));
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter, TRUE);
}
