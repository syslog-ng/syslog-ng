/*
 * Copyright (c) 2008-2016 Balabit
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
#include "libtest/msg_parse_lib.h"
#include "libtest/cr_template.h"

#include "logmatcher.h"
#include "apphook.h"
#include "plugin.h"
#include "cfg.h"
#include "scratch-buffers.h"

#include <stdlib.h>
#include <string.h>

static LogMessage *
_create_log_message(const gchar *log)
{
  LogMessage *msg;
  gchar buf[1024];
  NVHandle nonasciiz = log_msg_get_value_handle("NON-ASCIIZ");
  gssize msglen;

  msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_MESSAGE, log, -1);

  /* NOTE: we test how our matchers cope with non-zero terminated values. We don't change message_len, only the value */
  g_snprintf(buf, sizeof(buf), "%sAAAAAAAAAAAA", log_msg_get_value(msg, LM_V_MESSAGE, &msglen));
  log_msg_set_value_by_name(msg, "MESSAGE2", buf, -1);

  /* add a non-zero terminated indirect value which contains the whole message */
  log_msg_set_value_indirect(msg, nonasciiz, log_msg_get_value_handle("MESSAGE2"), 0, msglen);

  return msg;
}

static LogMatcher *
_construct_matcher(gint matcher_flags, LogMatcher *(*construct)(const LogMatcherOptions *options))
{
  LogMatcherOptions matcher_options;

  log_matcher_options_defaults(&matcher_options);
  matcher_options.flags = matcher_flags;

  return construct(&matcher_options);
}


void
testcase_match(const gchar *log, const gchar *pattern, gboolean expected_result, LogMatcher *m)
{
  LogMessage *msg;
  gboolean result;
  NVHandle nonasciiz = log_msg_get_value_handle("NON-ASCIIZ");
  gssize msglen;
  const gchar *value;

  msg = _create_log_message(log);
  log_matcher_compile(m, pattern, NULL);

  value = log_msg_get_value(msg, nonasciiz, &msglen);
  result = log_matcher_match(m, msg, nonasciiz, value, msglen);

  cr_assert_eq(result, expected_result,
               "pattern=%s, result=%d, expected=%d\n",
               pattern, result, expected_result);

  log_matcher_unref(m);
  log_msg_unref(msg);
}

void
testcase_replace(const gchar *log, const gchar *re, gchar *replacement, const gchar *expected_result, LogMatcher *m)
{
  LogMessage *msg;
  LogTemplate *r;
  gchar *result;
  gssize length;
  gssize msglen;
  NVHandle nonasciiz = log_msg_get_value_handle("NON-ASCIIZ");
  const gchar *value;

  msg = _create_log_message(log);
  log_matcher_compile(m, re, NULL);

  r = log_template_new(configuration, NULL);
  cr_assert(log_template_compile(r, replacement, NULL));

  NVTable *nv_table = nv_table_ref(msg->payload);
  value = log_msg_get_value(msg, nonasciiz, &msglen);
  result = log_matcher_replace(m, msg, nonasciiz, value, msglen, r, &length);
  value = log_msg_get_value(msg, nonasciiz, &msglen);
  nv_table_unref(nv_table);

  cr_assert_arr_eq((result ? result : value), expected_result, (result ? length : msglen),
                   "pattern=%s, result=%.*s, expected=%s\n",
                   re, (int) length, (result ? result : value), expected_result);

  g_free(result);

  log_template_unref(r);
  log_matcher_unref(m);
  log_msg_unref(msg);
}


void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
}

void
teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
  cfg_free(configuration);
}

TestSuite(matcher, .init = setup, .fini = teardown);

Test(matcher, pcre_regexp, .description = "PCRE regexp")
{
  testcase_replace("árvíztűrőtükörfúrógép", "árvíz",
                   "favíz", "favíztűrőtükörfúrógép", _construct_matcher(0, log_matcher_pcre_re_new));
  testcase_replace("árvíztűrőtükörfúrógép", "^tűrő",
                   "faró", "árvíztűrőtükörfúrógép", _construct_matcher(0, log_matcher_pcre_re_new));
  testcase_replace("árvíztűrőtükörfúrógép", "tűrő", "",
                   "árvíztükörfúrógép", _construct_matcher(0, log_matcher_pcre_re_new));
  testcase_replace("wikiwiki", "wi", "", "kiki",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("wikiwiki", "wi", "kuku", "kukukikukuki",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
}

Test(matcher, back_ref)
{
  testcase_replace("wikiwiki", "(wiki)\\1", "", "",
                   _construct_matcher(LMF_STORE_MATCHES, log_matcher_pcre_re_new));
}

Test(matcher, empty_global, .description = "empty match with global flag")
{
  testcase_replace("aa bb", "c*", "#", "#a#a# #b#b#",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("aa bb", "a*", "#", "## #b#b#",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));

  testcase_replace("aa bb", "c*", "#", "#a#a# #b#b#",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("aa bb", "a*", "?", "?? ?b?b?",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("aa", "aa|b*", "@", "@@",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("aa", "aa|b*", "@", "@", _construct_matcher(0,
                   log_matcher_pcre_re_new));
  testcase_replace("aa", "b*|aa", "@", "@@@",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("aa", "b*|aa", "@", "@aa", _construct_matcher(0,
                   log_matcher_pcre_re_new));

  testcase_replace("wikiwiki", "wi", "", "kiki",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("wikiwiki", "wi", "kuku", "kukukikukuki",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
}

Test(matcher, disable_jit)
{
  testcase_replace("árvíztűrőtükörfúrógép", "árvíz",
                   "favíz", "favíztűrőtükörfúrógép", _construct_matcher(LMF_DISABLE_JIT, log_matcher_pcre_re_new));
}

Test(matcher, string_match)
{
  testcase_replace("árvíztűrőtükörfúrógép", "árvíz",
                   "favíz", "favíztűrőtükörfúrógép", _construct_matcher(LMF_PREFIX, log_matcher_string_new));
  testcase_replace("árvíztűrőtükörfúrógép", "tűrő",
                   "faró", "árvízfarótükörfúrógép", _construct_matcher(LMF_SUBSTRING, log_matcher_string_new));
  testcase_replace("árvíztűrőtükörfúrógép", "tűrő", "",
                   "árvíztükörfúrógép", _construct_matcher(LMF_SUBSTRING, log_matcher_string_new));
  testcase_replace("árvíztűrőtükörfúrógép",
                   "árvíztűrőtükörfúrógép", "almafa", "almafa", _construct_matcher(0, log_matcher_string_new));
  testcase_replace("", "valami-amivel-nem-szabadna-matchelni",
                   "almafa", "", _construct_matcher(0, log_matcher_string_new));


  testcase_match("val", "valami-amivel-nem-szabadna-matchelni",
                 FALSE, _construct_matcher(0, log_matcher_string_new));
  testcase_match("", "valami-amivel-nem-szabadna-matchelni", FALSE,
                 _construct_matcher(0, log_matcher_string_new));
  testcase_match("", "valami-amivel-nem-szabadna-matchelni", 0,
                 _construct_matcher(LMF_PREFIX, log_matcher_string_new));
  testcase_match("", "valami-amivel-nem-szabadna-matchelni", 0,
                 _construct_matcher(LMF_SUBSTRING, log_matcher_string_new));

  testcase_match("match", "match", TRUE,
                 _construct_matcher(0, log_matcher_string_new));
  testcase_match("match", "ma", TRUE,
                 _construct_matcher(LMF_PREFIX, log_matcher_string_new));
  testcase_match("match", "tch", TRUE,
                 _construct_matcher(LMF_SUBSTRING, log_matcher_string_new));

  testcase_replace("abcdef", "ABCDEF", "qwerty", "qwerty",
                   _construct_matcher(LMF_PREFIX | LMF_ICASE, log_matcher_string_new));
  testcase_replace("abcdef", "BCD", "qwerty", "aqwertyef",
                   _construct_matcher(LMF_SUBSTRING | LMF_ICASE, log_matcher_string_new));
}

Test(matcher, glob_match)
{
  testcase_match("árvíztűrőtükörfúrógép", "árvíz*",
                 TRUE, _construct_matcher(0, log_matcher_glob_new));
  testcase_match("árvíztűrőtükörfúrógép", "*fúrógép",
                 TRUE, _construct_matcher(0, log_matcher_glob_new));
  testcase_match("árvíztűrőtükörfúrógép", "*fúró*",
                 TRUE, _construct_matcher(0, log_matcher_glob_new));
  testcase_match("árvíztűrőtükörfúrógép", "tükör",
                 FALSE, _construct_matcher(0, log_matcher_glob_new));
  testcase_match("árvíztűrőtükörfúrógép", "viziló",
                 FALSE, _construct_matcher(0, log_matcher_glob_new));
}

Test(matcher, iso88592_never, .description = "match in iso-8859-2 never matches")
{
  testcase_match("\xe1rv\xedzt\xfbr\xf5t\xfck\xf6rf\xfar\xf3g\xe9p",
                 "\xe1rv\xed*", FALSE, _construct_matcher(0, log_matcher_glob_new));
}

Test(matcher, replace)
{
  testcase_replace("árvíztűrőtükörfúrógép", "árvíz",
                   "favíz", "favíztűrőtükörfúrógép", _construct_matcher(0, log_matcher_pcre_re_new));
  testcase_replace("árvíztűrőtükörfúrógép", "^tűrő",
                   "faró", "árvíztűrőtükörfúrógép", _construct_matcher(0, log_matcher_pcre_re_new));
  testcase_replace("árvíztűrőtükörfúrógép", "tűrő", "",
                   "árvíztükörfúrógép", _construct_matcher(0, log_matcher_pcre_re_new));
  testcase_replace("wikiwiki", "(wiki)\\1", "", "",
                   _construct_matcher(0, log_matcher_pcre_re_new));
  /* back ref with perl style $1 */
  testcase_replace("wikiwiki", "(wiki).+", "#$1#", "#wiki#",
                   _construct_matcher(0, log_matcher_pcre_re_new));
}

Test(matcher, pcre812_incompatibility, .description = "tests a pcre 8.12 incompatibility")
{
  testcase_replace("wikiwiki",
                   "([[:digit:]]{1,3}\\.){3}[[:digit:]]{1,3}", "foo", "wikiwiki", _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
}

Test(matcher, test_matcher_sets_num_matches_upon_successful_matching)
{
  LogMatcherOptions matcher_options;
  LogMessage *msg;
  gboolean result;
  const gchar *msg_payload = "kiwi-wiki";

  msg = _create_log_message(msg_payload);

  cr_assert_eq(msg->num_matches, 0);

  log_matcher_options_defaults(&matcher_options);
  matcher_options.flags = LMF_STORE_MATCHES;
  LogMatcher *m = log_matcher_pcre_re_new(&matcher_options);
  log_matcher_compile(m, "^(kiwi).*", NULL);

  /* initial match, number of capture groups is 2: $0, $1 */
  gssize value_len;
  const gchar *value = log_msg_get_value(msg, LM_V_MESSAGE, &value_len);
  result = log_matcher_match(m, msg, LM_V_MESSAGE, value, value_len);
  cr_assert(result);

  assert_log_message_value(msg, LM_V_MESSAGE, msg_payload);
  assert_log_message_match_value(msg, 0, value);
  assert_log_message_match_value(msg, 1, "kiwi");

  cr_assert_eq(msg->num_matches, 2);

  /* another match, number of capture groups is 3, producing $0, $1, $2 */
  log_matcher_compile(m, "^(ki)(wi).*", NULL);
  value = log_msg_get_value(msg, LM_V_MESSAGE, &value_len);
  result = log_matcher_match(m, msg, LM_V_MESSAGE, value, value_len);
  cr_assert(result);

  assert_log_message_value(msg, LM_V_MESSAGE, msg_payload);
  assert_log_message_match_value(msg, 0, value);
  assert_log_message_match_value(msg, 1, "ki");
  assert_log_message_match_value(msg, 2, "wi");
  cr_assert_eq(msg->num_matches, 3);

  /* another match, decreasing the number of matches, going back to 2 capture groups */
  log_matcher_compile(m, "^(kiwi).*", NULL);
  value = log_msg_get_value(msg, LM_V_MESSAGE, &value_len);
  result = log_matcher_match(m, msg, LM_V_MESSAGE, value, value_len);
  cr_assert(result);

  assert_log_message_value(msg, LM_V_MESSAGE, msg_payload);
  assert_log_message_match_value(msg, 0, value);
  assert_log_message_match_value(msg, 1, "kiwi");
  cr_assert_eq(msg->num_matches, 2);

  log_matcher_unref(m);
  log_msg_unref(msg);
}

Test(matcher, test_matcher_captures_into_indirect_values)
{
  LogMatcherOptions matcher_options;
  LogMessage *msg;
  gboolean result;
  const gchar *msg_payload = "kiwi-wiki";

  msg = _create_log_message(msg_payload);

  log_matcher_options_defaults(&matcher_options);
  matcher_options.flags = LMF_STORE_MATCHES;
  LogMatcher *m = log_matcher_pcre_re_new(&matcher_options);
  log_matcher_compile(m, "^(?<foobar>kiwi).*", NULL);

  result = log_matcher_match_value(m, msg, LM_V_MESSAGE);
  cr_assert(result);

  assert_log_message_match_value(msg, 1, "kiwi");
  assert_log_message_value_by_name(msg, "foobar", "kiwi");
  assert_log_message_value_is_indirect(msg, log_msg_get_value_handle("foobar"));
  assert_log_message_value_is_indirect(msg, log_msg_get_value_handle("1"));
  log_matcher_unref(m);
  log_msg_unref(msg);
}

Test(matcher, test_matcher_builtins_are_captured_into_direct_values)
{
  LogMatcherOptions matcher_options;
  LogMessage *msg;
  gboolean result;
  const gchar *msg_payload = "kiwi-wiki";

  msg = _create_log_message(msg_payload);

  log_matcher_options_defaults(&matcher_options);
  matcher_options.flags = LMF_STORE_MATCHES;
  LogMatcher *m = log_matcher_pcre_re_new(&matcher_options);
  log_matcher_compile(m, "^(?<PID>kiwi).*", NULL);

  result = log_matcher_match_value(m, msg, LM_V_MESSAGE);
  cr_assert(result);

  assert_log_message_match_value(msg, 1, "kiwi");
  assert_log_message_value_by_name(msg, "PID", "kiwi");
  assert_log_message_value_is_direct(msg, log_msg_get_value_handle("PID"));
  assert_log_message_value_is_indirect(msg, log_msg_get_value_handle("1"));
  log_matcher_unref(m);
  log_msg_unref(msg);
}

Test(matcher, test_matcher_matches_against_macros_are_captured_directly)
{
  LogMatcherOptions matcher_options;
  LogMessage *msg;
  gboolean result;

  msg = create_empty_message();

  log_matcher_options_defaults(&matcher_options);
  matcher_options.flags = LMF_STORE_MATCHES;
  LogMatcher *m = log_matcher_pcre_re_new(&matcher_options);
  log_matcher_compile(m, "^(?<foobar>10\\.11\\.)12\\.13", NULL);

  result = log_matcher_match_value(m, msg, log_msg_get_value_handle("SOURCEIP"));
  cr_assert(result);

  assert_log_message_match_value(msg, 1, "10.11.");
  assert_log_message_value_by_name(msg, "foobar", "10.11.");
  assert_log_message_value_is_direct(msg, log_msg_get_value_handle("foobar"));
  assert_log_message_value_is_direct(msg, log_msg_get_value_handle("1"));
  log_matcher_unref(m);
  log_msg_unref(msg);
}

Test(matcher, test_matcher_matches_against_matches_are_captured_directly)
{
  LogMatcherOptions matcher_options;
  LogMessage *msg;
  gboolean result;

  msg = create_empty_message();
  log_msg_set_match(msg, 1, "kiwi-wiki", -1);

  log_matcher_options_defaults(&matcher_options);
  matcher_options.flags = LMF_STORE_MATCHES;
  LogMatcher *m = log_matcher_pcre_re_new(&matcher_options);
  log_matcher_compile(m, "^(?<foobar>kiwi).*", NULL);

  result = log_matcher_match_value(m, msg, log_msg_get_value_handle("1"));
  cr_assert(result);

  assert_log_message_match_value(msg, 1, "kiwi");
  assert_log_message_value_by_name(msg, "foobar", "kiwi");
  assert_log_message_value_is_direct(msg, log_msg_get_value_handle("foobar"));
  assert_log_message_value_is_direct(msg, log_msg_get_value_handle("1"));
  log_matcher_unref(m);
  log_msg_unref(msg);
}

Test(matcher, test_matcher_matches_against_buffers_are_captured_directly)
{
  LogMatcherOptions matcher_options;
  LogMessage *msg;
  gboolean result;

  msg = create_empty_message();

  log_matcher_options_defaults(&matcher_options);
  matcher_options.flags = LMF_STORE_MATCHES;
  LogMatcher *m = log_matcher_pcre_re_new(&matcher_options);
  log_matcher_compile(m, "^(?<foobar>kiwi).*", NULL);

  result = log_matcher_match_buffer(m, msg, "kiwi-wiki", 9);
  cr_assert(result);

  assert_log_message_match_value(msg, 1, "kiwi");
  assert_log_message_value_by_name(msg, "foobar", "kiwi");
  assert_log_message_value_is_direct(msg, log_msg_get_value_handle("foobar"));
  assert_log_message_value_is_direct(msg, log_msg_get_value_handle("1"));
  log_matcher_unref(m);
  log_msg_unref(msg);
}

Test(matcher, test_replace_works_correctly_if_capture_group_overwrites_the_input_in_a_match_variable)
{
  gssize value_len;
  gssize result_len;
  LogTemplate *replace_template;
  const gchar *test_message = "foo bar baz";
  const gchar *expected_result = "oo bar baz";
  const gchar *match_pattern = "^(\\w)";
  const gchar *replace_pattern = "";

  LogMatcher *m = _construct_matcher(0, log_matcher_pcre_re_new);
  log_matcher_compile(m, match_pattern, NULL);

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "1", test_message, -1);

  replace_template = log_template_new(configuration, NULL);
  cr_assert(log_template_compile(replace_template, replace_pattern, NULL));

  NVHandle input_handle = log_msg_get_value_handle("1");
  const gchar *input = log_msg_get_value(msg, input_handle, &value_len);

  NVTable *payload = nv_table_ref(msg->payload);
  gchar *result = log_matcher_replace(m, msg, input_handle, input, value_len,
                                      replace_template, &result_len);
  nv_table_unref(payload);
  cr_log_info("replace result value: %s, length(%ld)", result, result_len);
  cr_assert_arr_eq(result, expected_result, strlen(expected_result),
                   "replace failed; result: %s (length %ld), expected: %s (length %ld)",
                   result, result_len, expected_result, strlen(expected_result));

  g_free(result);
  log_template_unref(replace_template);
  log_matcher_unref(m);
  log_msg_unref(msg);
}

Test(matcher, test_replace_works_correctly_if_named_capture_group_overwrites_the_input)
{
  gssize value_len;
  gssize result_len;
  LogTemplate *replace_template;
  const gchar *test_message = "foo bar baz";
  const gchar *expected_result = "foo bar RRR";
  const gchar *match_pattern = "(?<TMP>(baz))";
  const gchar *replace_pattern = "RRR";

  LogMatcher *m = _construct_matcher(0, log_matcher_pcre_re_new);
  log_matcher_compile(m, match_pattern, NULL);

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "TMP", test_message, -1);

  replace_template = log_template_new(configuration, NULL);
  cr_assert(log_template_compile(replace_template, replace_pattern, NULL));

  NVHandle input_handle = log_msg_get_value_handle("TMP");
  const gchar *input = log_msg_get_value(msg, input_handle, &value_len);

  NVTable *payload = nv_table_ref(msg->payload);
  gchar *result = log_matcher_replace(m, msg, input_handle, input, value_len,
                                      replace_template, &result_len);
  nv_table_unref(payload);
  cr_log_info("replace result value: %s, length(%ld)", result, result_len);
  cr_assert_arr_eq(result, expected_result, strlen(expected_result),
                   "replace failed; result: %s (length %ld), expected: %s (length %ld)",
                   result, result_len, expected_result, strlen(expected_result));

  g_free(result);
  log_template_unref(replace_template);
  log_matcher_unref(m);
  log_msg_unref(msg);
}

Test(matcher, test_replace_works_correctly_if_input_is_a_match_value_that_gets_truncated)
{
  gssize value_len;
  gssize result_len;
  LogTemplate *replace_template;
  const gchar *test_message = "foo bar baz";
  const gchar *expected_result = "foo RRR baz";
  const gchar *match_pattern = "(bar)";
  const gchar *replace_pattern = "RRR";

  LogMatcher *m = _construct_matcher(0, log_matcher_pcre_re_new);
  log_matcher_compile(m, match_pattern, NULL);

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "2", test_message, -1);

  replace_template = log_template_new(configuration, NULL);
  cr_assert(log_template_compile(replace_template, replace_pattern, NULL));

  NVHandle input_handle = log_msg_get_value_handle("2");
  const gchar *input = log_msg_get_value(msg, input_handle, &value_len);

  NVTable *payload = nv_table_ref(msg->payload);
  gchar *result = log_matcher_replace(m, msg, input_handle, input, value_len,
                                      replace_template, &result_len);
  nv_table_unref(payload);
  cr_log_info("replace result value: %s, length(%ld)", result, result_len);
  cr_assert_arr_eq(result, expected_result, strlen(expected_result),
                   "replace failed; result: %s (length %ld), expected: %s (length %ld)",
                   result, result_len, expected_result, strlen(expected_result));

  g_free(result);
  log_template_unref(replace_template);
  log_matcher_unref(m);
  log_msg_unref(msg);
}
