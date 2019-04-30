/*
 * Copyright (c) 2010-2018 Balabit
 * Copyright (c) 2010-2015 Balázs Scheidler
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

#include "libtest/cr_template.h"
#include "libtest/grab-logging.h"
#include <criterion/criterion.h>

#include "apphook.h"
#include "plugin.h"
#include "cfg.h"
#include "logmsg/logmsg.h"
#include "scratch-buffers.h"

static void
add_dummy_template_to_configuration(void)
{
  LogTemplate *dummy = log_template_new(configuration, "dummy");
  cr_assert(log_template_compile(dummy, "dummy template expanded $HOST", NULL),
            "Unexpected error compiling dummy template");
  cfg_tree_add_template(&configuration->tree, dummy);
}

static void
_log_msg_free(gpointer data, gpointer user_data)
{
  log_msg_unref((LogMessage *) data);
}

static GPtrArray *
create_log_messages_with_values(const gchar *name, const gchar **values)
{
  LogMessage *message;
  GPtrArray *messages = g_ptr_array_new();

  const gchar **value;
  for (value = values; *value != NULL; ++value)
    {
      message = create_empty_message();
      log_msg_set_value_by_name(message, name, *value, -1);
      g_ptr_array_add(messages, message);
    }

  return messages;
}

static void
free_log_message_array(GPtrArray *messages)
{
  g_ptr_array_foreach(messages, _log_msg_free, NULL);
  g_ptr_array_free(messages, TRUE);
}

void
setup(void)
{
  app_startup();
  init_template_tests();
  add_dummy_template_to_configuration();
  cfg_load_module(configuration, "basicfuncs");
}

void
teardown(void)
{
  deinit_template_tests();
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(basicfuncs, .init = setup, .fini = teardown);

Test(basicfuncs, test_cond_funcs)
{
  assert_template_format_with_context("$(grep 'facility(local3)' $PID)", "23323,23323");
  assert_template_format_with_context("$(grep -m 1 'facility(local3)' $PID)", "23323");
  assert_template_format_with_context("$(grep 'facility(local3)' $PID $PROGRAM)", "23323,syslog-ng,23323,syslog-ng");
  assert_template_format_with_context("$(grep 'facility(local4)' $PID)", "");
  assert_template_format_with_context("$(grep ('$FACILITY' eq 'local4') $PID)", "");
  assert_template_format_with_context("$(grep ('$FACILITY(' eq 'local3(') $PID)", "23323,23323");
  assert_template_format_with_context("$(grep ('$FACILITY(' eq 'local4)') $PID)", "");
  assert_template_format_with_context("$(grep \\'$FACILITY\\'\\ eq\\ \\'local4\\' $PID)", "");

  assert_template_format_with_context("$(if 'facility(local4)' alma korte)", "korte");
  assert_template_format_with_context("$(if 'facility(local3)' alma korte)", "alma");

  assert_template_format_with_context("$(if '\"$FACILITY\" lt \"local3\"' alma korte)", "korte");
  assert_template_format_with_context("$(if '\"$FACILITY\" le \"local3\"' alma korte)", "alma");
  assert_template_format_with_context("$(if '\"$FACILITY\" eq \"local3\"' alma korte)", "alma");
  assert_template_format_with_context("$(if '\"$FACILITY\" ne \"local3\"' alma korte)", "korte");
  assert_template_format_with_context("$(if '\"$FACILITY\" gt \"local3\"' alma korte)", "korte");
  assert_template_format_with_context("$(if '\"$FACILITY\" ge \"local3\"' alma korte)", "alma");

  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" < \"19\"' alma korte)", "korte");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" <= \"19\"' alma korte)", "alma");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" == \"19\"' alma korte)", "alma");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" != \"19\"' alma korte)", "korte");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" > \"19\"' alma korte)", "korte");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" >= \"19\"' alma korte)", "alma");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" >= \"19\" and \"kicsi\" eq \"nagy\"' alma korte)",
                                      "korte");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" >= \"19\" or \"kicsi\" eq \"nagy\"' alma korte)", "alma");

  assert_template_format_with_context("$(grep 'facility(local3)' $PID)@0", "23323");
  assert_template_format_with_context("$(grep 'facility(local3)' $PID)@1", "23323");
  assert_template_format_with_context("$(grep 'facility(local3)' $PID)@2", "");

  assert_template_format_with_context("$(or 1 \"\" 2)", "1");
  assert_template_format_with_context("$(or \"\" 2)", "2");
  assert_template_format_with_context("$(or \"\" \"\")", "");
  assert_template_format_with_context("$(or)", "");
}

Test(basicfuncs, test_str_funcs)
{
  assert_template_format("$(ipv4-to-int $SOURCEIP)", "168496141");

  assert_template_format("$(length $HOST $PID)", "5 5");
  assert_template_format("$(length $HOST)", "5");
  assert_template_format("$(length)", "");

  assert_template_format("$(substr $HOST 1 3)", "zor");
  assert_template_format("$(substr $HOST 1)", "zorp");
  assert_template_format("$(substr $HOST -1)", "p");
  assert_template_format("$(substr $HOST -2 1)", "r");

  assert_template_format("$(strip ${APP.STRIP1})", "value");
  assert_template_format("$(strip ${APP.STRIP2})", "value");
  assert_template_format("$(strip ${APP.STRIP3})", "value");
  assert_template_format("$(strip ${APP.STRIP4})", "value");
  assert_template_format("$(strip ${APP.STRIP5})", "");

  assert_template_format("$(strip ${APP.STRIP5} ${APP.STRIP1} ${APP.STRIP5})", "value");
  assert_template_format("$(strip ${APP.STRIP1} ${APP.STRIP2} ${APP.STRIP3} ${APP.STRIP4} ${APP.STRIP5})",
                         "value value value value");
  assert_template_format("$(strip ŐRÜLT_ÍRÓ)", "ŐRÜLT_ÍRÓ"); /* Wide characters are accepted */
  assert_template_format("$(strip ' \n\t\r  a  b \n\t\r ')", "a  b");

  assert_template_format("$(sanitize alma/bela)", "alma_bela");
  assert_template_format("$(sanitize -r @ alma/bela)", "alma@bela");
  assert_template_format("$(sanitize -i @ alma@bela)", "alma_bela");
  assert_template_format("$(sanitize -i '@/l ' alma@/bela)", "a_ma__be_a");
  assert_template_format("$(sanitize alma\x1b_bela)", "alma__bela");
  assert_template_format("$(sanitize -C alma\x1b_bela)", "alma\x1b_bela");

  assert_template_format("$(sanitize $HOST $PROGRAM)", "bzorp/syslog-ng");
  assert_template_failure("$(sanitize ${missingbrace)", "Invalid macro, '}' is missing, error_pos='14'");

  assert_template_format("$(indent-multi-line 'foo\nbar')", "foo\n\tbar");

  assert_template_format("$(lowercase ŐRÜLT ÍRÓ)", "őrült író");
  assert_template_format("$(uppercase őrült író)", "ŐRÜLT ÍRÓ");

  assert_template_format("$(replace-delimiter \"\t\" \",\" \"hello\tworld\")", "hello,world");

  assert_template_format("$(padding foo 10)", "       foo");
  assert_template_format("$(padding foo 10 x)", "xxxxxxxfoo");
  assert_template_format("$(padding foo 10 abc)", "abcabcafoo");
  assert_template_format("$(padding foo 2)", "foo");        // longer macro than padding
  assert_template_format("$(padding foo 3)", "foo");        // len(macro) == padding length
  assert_template_format("$(padding foo 6 abc)", "abcfoo"); // len(padding string) == padding length
  assert_template_format("$(padding foo 4 '')", " foo");    // padding string == ''

  assert_template_failure("$(binary)", "Incorrect parameters");
  assert_template_failure("$(binary abc)", "unable to parse abc");
  assert_template_failure("$(binary 256)", "256 is above 255");
  assert_template_failure("$(binary 08)", "unable to parse 08");
  assert_template_format("$(binary 1)", "\1");
  assert_template_format("$(binary 1 0x1)", "\1\1");
  assert_template_format("$(binary 0xFF 255 0377)", "\xFF\xFF\xFF");
  assert_template_format_with_len("$(binary 0xFF 0x00 0x40)", "\xFF\000@", 3);

  assert_template_format("[$(base64-encode)]", "[]");
  assert_template_format("[$(base64-encode abc)]", "[YWJj]");
  assert_template_format("[$(base64-encode abcxyz)]", "[YWJjeHl6]");
  assert_template_format("[$(base64-encode abcd)]", "[YWJjZA==]");
  assert_template_format("[$(base64-encode abcdabcdabcdabcd)]", "[YWJjZGFiY2RhYmNkYWJjZA==]");
  assert_template_format("[$(base64-encode abcd abcd abcd abcd)]", "[YWJjZGFiY2RhYmNkYWJjZA==]");
  assert_template_format("[$(base64-encode 'X X')]", "[WCBY]");
  assert_template_format("[$(base64-encode xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx)]",
                         "[eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4"
                         "eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4"
                         "eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHg=]");
}

Test(basicfuncs, test_numeric_funcs)
{
  assert_template_format("$(+ $FACILITY_NUM 1)", "20");
  assert_template_format("$(+ -1 -1)", "-2");
  assert_template_format("$(- $FACILITY_NUM 1)", "18");
  assert_template_format("$(- $FACILITY_NUM 20)", "-1");
  assert_template_format("$(* $FACILITY_NUM 2)", "38");
  assert_template_format("$(/ $FACILITY_NUM 2)", "9");
  assert_template_format("$(% $FACILITY_NUM 3)", "1");
  assert_template_format("$(/ $FACILITY_NUM 0)", "NaN");
  assert_template_format("$(% $FACILITY_NUM 0)", "NaN");
  assert_template_format("$(+ foo bar)", "NaN");
  assert_template_format("$(/ 2147483648 1)", "2147483648");
  assert_template_format("$(+ 5000000000 5000000000)", "10000000000");
  assert_template_format("$(% 10000000000 5000000001)", "4999999999");
  assert_template_format("$(* 5000000000 2)", "10000000000");
  assert_template_format("$(- 10000000000 5000000000)", "5000000000");
}

Test(basicfuncs, test_fname_funcs)
{
  assert_template_format("$(basename foo)", "foo");
  assert_template_format("$(basename /foo/bar)", "bar");
  assert_template_format("$(basename /foo/bar/baz)", "baz");

  assert_template_format("$(dirname foo)", ".");
  assert_template_format("$(dirname /foo/bar)", "/foo");
  assert_template_format("$(dirname /foo/bar/)", "/foo/bar");
  assert_template_format("$(dirname /foo/bar/baz)", "/foo/bar");
}

typedef struct
{
  const gchar *macro;
  const gchar *result;
} MacroAndResult;

static void
_test_macros_with_context(const gchar *id, const gchar *numbers[], const MacroAndResult test_cases[])
{
  GPtrArray *messages = create_log_messages_with_values(id, numbers);

  start_grabbing_messages();
  for (const MacroAndResult *test_case = test_cases; test_case->macro; test_case++)
    assert_template_format_with_context_msgs(
      test_case->macro, test_case->result,
      (LogMessage **)messages->pdata, messages->len);

  stop_grabbing_messages();
  free_log_message_array(messages);
}

Test(basicfuncs, test_numeric_aggregate_simple)
{
  _test_macros_with_context(
    "NUMBER", (const gchar *[])
  { "1", "-1", "3", NULL
  },
  (const MacroAndResult[])
  {
    { "$(sum ${NUMBER})", "3" },
    { "$(min ${NUMBER})", "-1" },
    { "$(max ${NUMBER})", "3" },
    { "$(average ${NUMBER})", "1" },
    { }
  });
}

Test(basicfuncs, test_numeric_aggregate_invalid_values)
{
  _test_macros_with_context(
    "NUMBER", (const gchar *[])
  { "abc", "1", "c", "2", "", NULL
  },
  (const MacroAndResult[])
  {
    { "$(sum ${NUMBER})", "3" },
    { "$(min ${NUMBER})", "1" },
    { "$(max ${NUMBER})", "2" },
    { "$(average ${NUMBER})", "1" },
    { }
  });
}

Test(basicfuncs, test_numeric_aggregate_full_invalid_values)
{
  _test_macros_with_context(
    "NUMBER", (const gchar *[])
  { "abc", "184467440737095516160", "c", "", NULL
  },
  (const MacroAndResult[])
  {
    { "$(sum ${NUMBER})", "" },
    { "$(min ${NUMBER})", "" },
    { "$(max ${NUMBER})", "" },
    { "$(average ${NUMBER})", "" },
    { }
  });
}

Test(basicfuncs, test_misc_funcs)
{
  unsetenv("OHHELLO");
  setenv("TEST_ENV", "test-env", 1);

  assert_template_format("$(env OHHELLO)", "");
  assert_template_format("$(env TEST_ENV)", "test-env");
}

Test(basicfuncs, test_tf_template)
{
  assert_template_format("foo $(template dummy) bar", "foo dummy template expanded bzorp bar");
  assert_template_failure("foo $(template unknown) bar", "Unknown template function or template \"unknown\"");
}

Test(basicfuncs, test_list_funcs)
{
  assert_template_format("$(list-concat)", "");
  assert_template_format("$(list-concat foo bar baz)", "foo,bar,baz");
  assert_template_format("$(list-concat foo bar baz '')", "foo,bar,baz");
  assert_template_format("$(list-concat foo $HOST $PROGRAM $PID bar)", "foo,bzorp,syslog-ng,23323,bar");
  assert_template_format("$(list-concat foo $HOST,$PROGRAM,$PID bar)", "foo,bzorp,syslog-ng,23323,bar");
  assert_template_format("$(list-concat foo '$HOST,$PROGRAM,$PID' bar)", "foo,bzorp,syslog-ng,23323,bar");
  assert_template_format("$(list-concat foo '$HOST,$PROGRAM,$PID,' bar)", "foo,bzorp,syslog-ng,23323,bar");

  assert_template_format("$(list-append)", "");
  assert_template_format("$(list-append '' foo)", "foo");
  assert_template_format("$(list-append '' foo bar)", "foo,bar");
  assert_template_format("$(list-append '' foo bar baz)", "foo,bar,baz");
  assert_template_format("$(list-append foo,bar,baz 'x')", "foo,bar,baz,x");
  assert_template_format("$(list-append foo,bar,baz '')", "foo,bar,baz,\"\"");
  assert_template_format("$(list-append foo,bar,baz 'xxx,')", "foo,bar,baz,\"xxx,\"");
  assert_template_format("$(list-append foo,bar,baz 'a\tb')", "foo,bar,baz,\"a\\tb\"");

  assert_template_format("$(list-head)", "");
  assert_template_format("$(list-head '')", "");
  assert_template_format("$(list-head foo)", "foo");
  assert_template_format("$(list-head foo,)", "foo");
  assert_template_format("$(list-head foo,bar)", "foo");
  assert_template_format("$(list-head foo,bar,baz)", "foo");
  assert_template_format("$(list-head ,bar,baz)", "bar");

  assert_template_format("$(list-head foo bar)", "foo");
  assert_template_format("$(list-head foo bar baz)", "foo");
  assert_template_format("$(list-head '' bar baz)", "bar");

  assert_template_format("$(list-head '\"\\tfoo,\",bar,baz')", "\tfoo,");

  assert_template_format("$(list-nth 0 '\"foo,\",\"bar\",\"baz\"')", "foo,");
  assert_template_format("$(list-nth 1 '\"foo,\",\"bar\",\"baz\"')", "bar");
  assert_template_format("$(list-nth 2 '\"foo,\",\"bar\",\"baz\"')", "baz");
  assert_template_format("$(list-nth 3 '\"foo,\",\"bar\",\"baz\"')", "");
  assert_template_format("$(list-nth 4 '\"foo,\",\"bar\",\"baz\"')", "");
  assert_template_format("$(list-nth -1 '\"foo,\",\"bar\",\"baz\"')", "baz");
  assert_template_format("$(list-nth -2 '\"foo,\",\"bar\",\"baz\"')", "bar");
  assert_template_format("$(list-nth -3 '\"foo,\",\"bar\",\"baz\"')", "foo,");
  assert_template_format("$(list-nth -4 '\"foo,\",\"bar\",\"baz\"')", "");

  assert_template_format("$(list-tail)", "");
  assert_template_format("$(list-tail foo)", "");
  assert_template_format("$(list-tail foo,bar)", "bar");
  assert_template_format("$(list-tail foo,)", "");
  assert_template_format("$(list-tail ,bar)", "");
  assert_template_format("$(list-tail foo,bar,baz)", "bar,baz");
  assert_template_format("$(list-tail foo bar baz)", "bar,baz");
  assert_template_format("$(list-tail foo,bar baz bad)", "bar,baz,bad");
  assert_template_format("$(list-tail foo,bar,xxx, baz bad)", "bar,xxx,baz,bad");

  assert_template_format("$(list-slice 0:0 foo,bar,xxx,baz,bad)", "");
  assert_template_format("$(list-slice 0:1 foo,bar,xxx,baz,bad)", "foo");
  assert_template_format("$(list-slice 0:2 foo,bar,xxx,baz,bad)", "foo,bar");
  assert_template_format("$(list-slice 0:3 foo,bar,xxx,baz,bad)", "foo,bar,xxx");
  assert_template_format("$(list-slice 1:1 foo,bar,xxx,baz,bad)", "");
  assert_template_format("$(list-slice 1:2 foo,bar,xxx,baz,bad)", "bar");

  assert_template_format("$(list-slice : foo,bar,xxx,baz,bad)", "foo,bar,xxx,baz,bad");

  assert_template_format("$(list-slice 0: foo,bar,xxx,baz,bad)", "foo,bar,xxx,baz,bad");
  assert_template_format("$(list-slice 3: foo,bar,xxx,baz,bad)", "baz,bad");

  assert_template_format("$(list-slice :1 foo,bar,xxx,baz,bad)", "foo");
  assert_template_format("$(list-slice :2 foo,bar,xxx,baz,bad)", "foo,bar");
  assert_template_format("$(list-slice :3 foo,bar,xxx,baz,bad)", "foo,bar,xxx");

  assert_template_format("$(list-slice -1: foo,bar,xxx,baz,bad)", "bad");
  assert_template_format("$(list-slice -2: foo,bar,xxx,baz,bad)", "baz,bad");
  assert_template_format("$(list-slice -3: foo,bar,xxx,baz,bad)", "xxx,baz,bad");
  assert_template_format("$(list-slice -5: foo,bar,xxx,baz,bad)", "foo,bar,xxx,baz,bad");
  assert_template_format("$(list-slice -6: foo,bar,xxx,baz,bad)", "foo,bar,xxx,baz,bad");
  assert_template_format("$(list-slice -100: foo,bar,xxx,baz,bad)", "foo,bar,xxx,baz,bad");
  assert_template_format("$(list-slice :-1 foo,bar,xxx,baz,bad)", "foo,bar,xxx,baz");
  assert_template_format("$(list-slice :-2 foo,bar,xxx,baz,bad)", "foo,bar,xxx");
  assert_template_format("$(list-slice :-3 foo,bar,xxx,baz,bad)", "foo,bar");
  assert_template_format("$(list-slice :-4 foo,bar,xxx,baz,bad)", "foo");
  assert_template_format("$(list-slice :-5 foo,bar,xxx,baz,bad)", "");
  assert_template_format("$(list-slice :-6 foo,bar,xxx,baz,bad)", "");

  assert_template_format("$(list-count foo,bar,xxx, baz bad)", "5");
}

Test(basicfuncs, test_context_funcs)
{
  assert_template_format_with_context("$(context-length)", "2");

  assert_template_format_with_context("$(context-lookup 'facility(local3)' $PID)", "23323,23323");
  assert_template_format_with_context("$(context-lookup 'facility(local3)' ${comma_value})",
                                      "\"value,with,a,comma\",\"value,with,a,comma\"");

  assert_template_format_with_context("$(context-values ${PID})", "23323,23323");
  assert_template_format_with_context("$(context-values ${comma_value})",
                                      "\"value,with,a,comma\",\"value,with,a,comma\"");
}


Test(basicfuncs, test_tfurlencode)
{
  assert_template_format("$(url-encode '')", "");
  assert_template_format("$(url-encode test)", "test");
  assert_template_format("$(url-encode <>)", "%3C%3E");
  assert_template_format("$(url-encode &)", "%26");
}

Test(basicfuncs, test_tfurldecode)
{
  assert_template_format("$(url-decode '')", "");
  assert_template_format("$(url-decode test)", "test");
  assert_template_format("$(url-decode %3C%3E)", "<>");
  assert_template_format("$(url-decode %26)", "&");
  assert_template_format("$(url-decode %26 %26)", "&&");

  assert_template_format("$(url-decode %)", "");
  assert_template_format("$(url-decode %00a)", "");
}
