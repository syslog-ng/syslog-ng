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

#include "logmatcher.h"
#include "apphook.h"
#include "plugin.h"
#include "cfg.h"
#include "msg_parse_lib.h"

#include <stdlib.h>
#include <string.h>

static LogMessage *
_create_log_message(const gchar *log)
{
  LogMessage *msg;
  gchar buf[1024];
  NVHandle nonasciiz = log_msg_get_value_handle("NON-ASCIIZ");
  gssize msglen;

  msg = log_msg_new(log, strlen(log), &parse_options);

  /* NOTE: we test how our matchers cope with non-zero terminated values. We don't change message_len, only the value */
  g_snprintf(buf, sizeof(buf), "%sAAAAAAAAAAAA", log_msg_get_value(msg, LM_V_MESSAGE, &msglen));
  log_msg_set_value_by_name(msg, "MESSAGE2", buf, -1);

  /* add a non-zero terminated indirect value which contains the whole message */
  log_msg_set_value_indirect(msg, nonasciiz, log_msg_get_value_handle("MESSAGE2"), 0, 0, msglen);

  return msg;
}

static LogMatcher *
_construct_matcher(gint matcher_flags, LogMatcher *(*construct)(GlobalConfig *cfg, const LogMatcherOptions *options))
{
  LogMatcherOptions matcher_options;

  log_matcher_options_defaults(&matcher_options);
  matcher_options.flags = matcher_flags;

  return construct(configuration, &matcher_options);
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
  log_template_compile(r, replacement, NULL);

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
  init_and_load_syslogformat_module();
}

void
teardown(void)
{
  deinit_syslogformat_module();
  app_shutdown();
}

TestSuite(matcher, .init = setup, .fini = teardown);

Test(matcher, pcre_regexp, .description = "PCRE regexp")
{
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "árvíz",
                   "favíz", "favíztűrőtükörfúrógép", _construct_matcher(0, log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "^tűrő",
                   "faró", "árvíztűrőtükörfúrógép", _construct_matcher(0, log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "tűrő", "",
                   "árvíztükörfúrógép", _construct_matcher(0, log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: wikiwiki", "wi", "", "kiki",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: wikiwiki", "wi", "kuku", "kukukikukuki",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
}

Test(matcher, back_ref, .description = "back references are not portable, they work only on Linux")
{
#if __linux__
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: wikiwiki", "(wiki)\\1", "", "",
                   _construct_matcher(LMF_STORE_MATCHES, log_matcher_pcre_re_new));
#endif
}

Test(matcher, empty_global, .description = "empty match with global flag")
{
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: aa bb", "c*", "#", "#a#a# #b#b#",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: aa bb", "a*", "#", "## #b#b#",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));

  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: aa bb", "c*", "#", "#a#a# #b#b#",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: aa bb", "a*", "?", "?? ?b?b?",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: aa", "aa|b*", "@", "@@",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: aa", "aa|b*", "@", "@", _construct_matcher(0,
                   log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: aa", "b*|aa", "@", "@@@",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: aa", "b*|aa", "@", "@aa", _construct_matcher(0,
                   log_matcher_pcre_re_new));

  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: wikiwiki", "wi", "", "kiki",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: wikiwiki", "wi", "kuku", "kukukikukuki",
                   _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
}

Test(matcher, string_match)
{
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "árvíz",
                   "favíz", "favíztűrőtükörfúrógép", _construct_matcher(LMF_PREFIX, log_matcher_string_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "tűrő",
                   "faró", "árvízfarótükörfúrógép", _construct_matcher(LMF_SUBSTRING, log_matcher_string_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "tűrő", "",
                   "árvíztükörfúrógép", _construct_matcher(LMF_SUBSTRING, log_matcher_string_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép",
                   "árvíztűrőtükörfúrógép", "almafa", "almafa", _construct_matcher(0, log_matcher_string_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: ", "valami-amivel-nem-szabadna-matchelni",
                   "almafa", "", _construct_matcher(0, log_matcher_string_new));


  testcase_match("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: val", "valami-amivel-nem-szabadna-matchelni",
                 FALSE, _construct_matcher(0, log_matcher_string_new));
  testcase_match("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: ", "valami-amivel-nem-szabadna-matchelni", FALSE,
                 _construct_matcher(0, log_matcher_string_new));
  testcase_match("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: ", "valami-amivel-nem-szabadna-matchelni", 0,
                 _construct_matcher(LMF_PREFIX, log_matcher_string_new));
  testcase_match("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: ", "valami-amivel-nem-szabadna-matchelni", 0,
                 _construct_matcher(LMF_SUBSTRING, log_matcher_string_new));

  testcase_match("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: match", "match", TRUE, _construct_matcher(0,
                 log_matcher_string_new));
  testcase_match("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: match", "ma", TRUE,
                 _construct_matcher(LMF_PREFIX, log_matcher_string_new));
  testcase_match("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: match", "tch", TRUE,
                 _construct_matcher(LMF_SUBSTRING, log_matcher_string_new));

  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: abcdef", "ABCDEF", "qwerty", "qwerty",
                   _construct_matcher(LMF_PREFIX | LMF_ICASE, log_matcher_string_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: abcdef", "BCD", "qwerty", "aqwertyef",
                   _construct_matcher(LMF_SUBSTRING | LMF_ICASE, log_matcher_string_new));
}

Test(matcher, glob_match)
{
  testcase_match("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "árvíz*",
                 TRUE, _construct_matcher(0, log_matcher_glob_new));
  testcase_match("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "*fúrógép",
                 TRUE, _construct_matcher(0, log_matcher_glob_new));
  testcase_match("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "*fúró*",
                 TRUE, _construct_matcher(0, log_matcher_glob_new));
  testcase_match("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "tükör",
                 FALSE, _construct_matcher(0, log_matcher_glob_new));
  testcase_match("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "viziló",
                 FALSE, _construct_matcher(0, log_matcher_glob_new));
}

Test(matcher, iso88592_never, .description = "match in iso-8859-2 never matches")
{
  testcase_match("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: \xe1rv\xedzt\xfbr\xf5t\xfck\xf6rf\xfar\xf3g\xe9p",
                 "\xe1rv\xed*", FALSE, _construct_matcher(0, log_matcher_glob_new));
}

Test(matcher, replace)
{
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "árvíz",
                   "favíz", "favíztűrőtükörfúrógép", _construct_matcher(0, log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "^tűrő",
                   "faró", "árvíztűrőtükörfúrógép", _construct_matcher(0, log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép", "tűrő", "",
                   "árvíztükörfúrógép", _construct_matcher(0, log_matcher_pcre_re_new));
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: wikiwiki", "(wiki)\\1", "", "",
                   _construct_matcher(0, log_matcher_pcre_re_new));
  /* back ref with perl style $1 */
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: wikiwiki", "(wiki).+", "#$1#", "#wiki#",
                   _construct_matcher(0, log_matcher_pcre_re_new));
}

Test(matcher, pcre812_incompatibility, .description = "tests a pcre 8.12 incompatibility")
{
  testcase_replace("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: wikiwiki",
                   "([[:digit:]]{1,3}\\.){3}[[:digit:]]{1,3}", "foo", "wikiwiki", _construct_matcher(LMF_GLOBAL, log_matcher_pcre_re_new));
}
