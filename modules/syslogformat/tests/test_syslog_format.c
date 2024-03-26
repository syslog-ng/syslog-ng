/*
 * Copyright (c) 2022 One Identity
 * Copyright (c) 2022 László Várady
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

#include "apphook.h"
#include "cfg.h"
#include "syslog-format.h"
#include "logmsg/logmsg.h"
#include "msg-format.h"
#include "scratch-buffers.h"

#include <string.h>

GlobalConfig *cfg;
MsgFormatOptions parse_options;

static void
setup(void)
{
  app_startup();
  syslog_format_init();

  cfg = cfg_new_snippet();
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, cfg);
}

static void
teardown(void)
{
  msg_format_options_destroy(&parse_options);
  scratch_buffers_explicit_gc();
  app_shutdown();
  cfg_free(cfg);
}

TestSuite(syslog_format, .init = setup, .fini = teardown);

Test(syslog_format, parser_should_not_spin_on_non_zero_terminated_input, .timeout = 10)
{
  const gchar *data = "<182>2022-08-17T05:02:28.217 mymachine su: 'su root' failed for lonvick on /dev/pts/8";
  /* chosen carefully to reproduce a bug */
  gsize data_length = 27;

  LogMessage *msg = log_msg_new_empty();

  gsize problem_position;
  cr_assert(syslog_format_handler(&parse_options, msg, (const guchar *) data, data_length, &problem_position));

  log_msg_unref(msg);
}

Test(syslog_format, cisco_sequence_id_non_zero_termination)
{
  const gchar *data = "<189>65536: ";
  gsize data_length = strlen(data);

  LogMessage *msg = log_msg_new_empty();

  gsize problem_position;
  cr_assert(syslog_format_handler(&parse_options, msg, (const guchar *) data, data_length, &problem_position));
  cr_assert_str_eq(log_msg_get_value_by_name(msg, ".SDATA.meta.sequenceId", NULL), "65536");

  log_msg_unref(msg);
}

Test(syslog_format, rfc3164_style_message_when_parsed_as_rfc5424_is_marked_as_such_in_msgformat)
{
  const gchar *data = "<189>Feb  3 12:34:56 host program[pid]: message";
  gsize data_length = strlen(data);

  parse_options.flags |= LP_SYSLOG_PROTOCOL;
  LogMessage *msg = log_msg_new_empty();

  gsize problem_position;
  cr_assert(syslog_format_handler(&parse_options, msg, (const guchar *) data, data_length, &problem_position));
  assert_log_message_value_by_name(msg, "HOST", "host");
  assert_log_message_value_by_name(msg, "PROGRAM", "program");
  assert_log_message_value_by_name(msg, "PID", "pid");
  assert_log_message_value_by_name(msg, "MSG", "message");
  assert_log_message_value_by_name(msg, "MSGFORMAT", "rfc3164");

  log_msg_unref(msg);
}

Test(syslog_format, rfc5424_style_message_when_parsed_as_rfc5424_is_marked_as_such_in_msgformat)
{
  const gchar *data = "<189>1 2024-02-03T12:34:56Z host program pid msgid [foo bar=baz] message";
  gsize data_length = strlen(data);

  parse_options.flags |= LP_SYSLOG_PROTOCOL;
  LogMessage *msg = log_msg_new_empty();

  gsize problem_position;
  cr_assert(syslog_format_handler(&parse_options, msg, (const guchar *) data, data_length, &problem_position));
  assert_log_message_value_by_name(msg, "HOST", "host");
  assert_log_message_value_by_name(msg, "PROGRAM", "program");
  assert_log_message_value_by_name(msg, "PID", "pid");
  assert_log_message_value_by_name(msg, "MSGID", "msgid");
  assert_log_message_value_by_name(msg, "MSG", "message");
  assert_log_message_value_by_name(msg, "MSGFORMAT", "rfc5424");

  log_msg_unref(msg);
}

Test(syslog_format, minimal_non_zero_terminated_numeric_message_is_parsed_as_program_name)
{
  const gchar *data = "<189>65536";
  gsize data_length = strlen(data);

  LogMessage *msg = log_msg_new_empty();

  gsize problem_position;
  cr_assert(syslog_format_handler(&parse_options, msg, (const guchar *) data, data_length, &problem_position));
  cr_assert_str_eq(log_msg_get_value_by_name(msg, "PROGRAM", NULL), "65536");

  log_msg_unref(msg);
}

static gboolean
_extract_sdata_into_message_with_prefix(const gchar *sdata, LogMessage **pmsg, const gchar *sdata_prefix)
{
  const guchar *data = (const guchar *) sdata;
  gint data_length = strlen(sdata);
  LogMessage *msg = log_msg_new_empty();
  MsgFormatOptions local_parse_options = {0};

  msg_format_options_defaults(&local_parse_options);
  msg_format_options_set_sdata_prefix(&local_parse_options, sdata_prefix);
  msg_format_options_init(&local_parse_options, cfg);
  gboolean result = _syslog_format_parse_sd(msg, &data, &data_length, &local_parse_options);
  msg_format_options_destroy(&local_parse_options);

  if (pmsg)
    *pmsg = msg;
  else
    log_msg_unref(msg);

  return result;
}

static gboolean
_extract_sdata_into_message(const gchar *sdata, LogMessage **pmsg)
{
  return _extract_sdata_into_message_with_prefix(sdata, pmsg, NULL);
}

Test(syslog_format, test_sdata_dash_means_there_are_no_sdata_elements)
{
  LogMessage *msg;

  cr_assert(_extract_sdata_into_message("-", &msg));
  assert_log_message_value_by_name(msg, "SDATA", "");
  log_msg_unref(msg);
}

Test(syslog_format, test_sdata_parsing_invalid_brackets_are_returned_as_failures)
{
  cr_assert_not(_extract_sdata_into_message("<", NULL));
  cr_assert_not(_extract_sdata_into_message("[", NULL));
  cr_assert_not(_extract_sdata_into_message("[]", NULL));
  cr_assert_not(_extract_sdata_into_message("]", NULL));
  cr_assert_not(_extract_sdata_into_message("[foobar", NULL));
}

Test(syslog_format, test_sdata_id_without_param_is_accepted_and_represented_in_sdata)
{
  LogMessage *msg;
  cr_assert(_extract_sdata_into_message("[foobar]", &msg));
  assert_log_message_value_by_name(msg, "SDATA", "[foobar]");
  assert_log_message_value_by_name(msg, ".SDATA.foobar", "");
  log_msg_unref(msg);
}

Test(syslog_format, test_sdata_simple_id_and_param)
{
  LogMessage *msg;
  cr_assert(_extract_sdata_into_message("[foo bar=\"baz\"]", &msg));
  assert_log_message_value_by_name(msg, "SDATA", "[foo bar=\"baz\"]");
  assert_log_message_value_by_name(msg, ".SDATA.foo.bar", "baz");
  log_msg_unref(msg);
}

Test(syslog_format, test_sdata_invalid_sd_id_and_param)
{
  cr_assert_not(_extract_sdata_into_message("[foo= bar=\"baz\"]", NULL));
  cr_assert_not(_extract_sdata_into_message("[foo\" bar=\"baz\"]", NULL));
}

Test(syslog_format, test_sdata_invalid_param)
{
  cr_assert_not(_extract_sdata_into_message("[foo bar\"=\"baz\"]", NULL));
  cr_assert_not(_extract_sdata_into_message("[foo bar]", NULL));
  cr_assert_not(_extract_sdata_into_message("[foo bar baz]", NULL));
}

Test(syslog_format, test_sdata_invalid_value)
{
  cr_assert_not(_extract_sdata_into_message("[foo bar=\"]", NULL));
}

Test(syslog_format, test_sdata_unquoted_value)
{
  LogMessage *msg;

  /* VMWare spits out SDATA like this: [Originator@6876 sub=Vimsvc.ha-eventmgr opID=esxui-13c6-6b16 sid=5214bde6 user=root] */
  cr_assert(_extract_sdata_into_message("[foo bar=baz]", &msg));
  assert_log_message_value_by_name(msg, "SDATA", "[foo bar=\"baz\"]");
  assert_log_message_value_by_name(msg, ".SDATA.foo.bar", "baz");
  log_msg_unref(msg);

  cr_assert(
    _extract_sdata_into_message("[Originator@6876 sub=Vimsvc.ha-eventmgr opID=esxui-13c6-6b16 sid=5214bde6 user=root]",
                                &msg));
  assert_log_message_value_by_name(msg, "SDATA",
                                   "[Originator@6876 sub=\"Vimsvc.ha-eventmgr\" opID=\"esxui-13c6-6b16\" sid=\"5214bde6\" user=\"root\"]");
  assert_log_message_value_by_name(msg, ".SDATA.Originator@6876.sub", "Vimsvc.ha-eventmgr");
  assert_log_message_value_by_name(msg, ".SDATA.Originator@6876.opID", "esxui-13c6-6b16");
  assert_log_message_value_by_name(msg, ".SDATA.Originator@6876.sid", "5214bde6");
  assert_log_message_value_by_name(msg, ".SDATA.Originator@6876.user", "root");
  log_msg_unref(msg);
}

Test(syslog_format, test_sdata_param_value_invalid_escape)
{
  LogMessage *msg;
  cr_assert(_extract_sdata_into_message("[foo bar=\"\\a\"]", &msg));

  /* non-special character follows a backslash.  We extract as if they were
   * normal characters (as the spec), however when we reconstruct the SDATA
   * string, we regenerate it from our internal value, which "fixes" it.
   * This is against the spec, but our approach is to parse and reconstruct
   * and there's no way we can encode errors into our internal
   * representation. There would be not too much value either. */
  assert_log_message_value_by_name(msg, "SDATA", "[foo bar=\"\\\\a\"]");
  assert_log_message_value_by_name(msg, ".SDATA.foo.bar", "\\a");
  log_msg_unref(msg);
}

Test(syslog_format, test_sdata_param_value_escape_quote)
{
  LogMessage *msg;
  cr_assert(_extract_sdata_into_message("[foo bar=\"\\\"\"]", &msg));

  assert_log_message_value_by_name(msg, "SDATA", "[foo bar=\"\\\"\"]");
  assert_log_message_value_by_name(msg, ".SDATA.foo.bar", "\"");
  log_msg_unref(msg);
}

Test(syslog_format, test_sdata_param_value_escape_bracket)
{
  LogMessage *msg;
  cr_assert(_extract_sdata_into_message("[foo bar=\"\\]\"]", &msg));

  assert_log_message_value_by_name(msg, "SDATA", "[foo bar=\"\\]\"]");
  assert_log_message_value_by_name(msg, ".SDATA.foo.bar", "]");
  log_msg_unref(msg);
}

Test(syslog_format, test_sdata_param_value_escape_backslash)
{
  LogMessage *msg;
  cr_assert(_extract_sdata_into_message("[foo bar=\"\\\\\"]", &msg));

  assert_log_message_value_by_name(msg, "SDATA", "[foo bar=\"\\\\\"]");
  assert_log_message_value_by_name(msg, ".SDATA.foo.bar", "\\");
  log_msg_unref(msg);
}

Test(syslog_format, test_sdata_simple_id_and_multiple_params)
{
  LogMessage *msg;
  cr_assert(_extract_sdata_into_message("[foo bar=\"baz\" chew=\"chow\" peek=\"poke\"]", &msg));
  assert_log_message_value_by_name(msg, "SDATA", "[foo bar=\"baz\" chew=\"chow\" peek=\"poke\"]");
  assert_log_message_value_by_name(msg, ".SDATA.foo.bar", "baz");
  assert_log_message_value_by_name(msg, ".SDATA.foo.chew", "chow");
  assert_log_message_value_by_name(msg, ".SDATA.foo.peek", "poke");
  log_msg_unref(msg);
}

Test(syslog_format, test_sdata_enterprise_qualified_id_with_and_multiple_params)
{
  LogMessage *msg;
  cr_assert(_extract_sdata_into_message("[foo@1.2.3.4 bar=\"baz\" chew=\"chow\" peek=\"poke\"]", &msg));
  assert_log_message_value_by_name(msg, "SDATA", "[foo@1.2.3.4 bar=\"baz\" chew=\"chow\" peek=\"poke\"]");
  assert_log_message_value_by_name(msg, ".SDATA.foo@1.2.3.4.bar", "baz");
  assert_log_message_value_by_name(msg, ".SDATA.foo@1.2.3.4.chew", "chow");
  assert_log_message_value_by_name(msg, ".SDATA.foo@1.2.3.4.peek", "poke");
  log_msg_unref(msg);
}

Test(syslog_format, test_sdata_missing_closing_bracket)
{
  cr_assert_not(_extract_sdata_into_message("[foo bar=\"baz\"", NULL));
}

Test(syslog_format, test_long_sdata)
{
  const gchar *sdata_prefix = ".SDATA.";
  gchar long_sdata[1024] = "[";

  gsize long_sdata_id = 255 - strlen(sdata_prefix);
  memset(long_sdata + 1, 'a', long_sdata_id);
  strcpy(long_sdata + 1 + long_sdata_id, " a=b]");
  cr_expect_not(_extract_sdata_into_message_with_prefix(long_sdata, NULL, sdata_prefix));

  long_sdata_id -= 1;
  memset(long_sdata + 1, 'a', long_sdata_id);
  strcpy(long_sdata + 1 + long_sdata_id, " a=b]");
  cr_expect_not(_extract_sdata_into_message_with_prefix(long_sdata, NULL, sdata_prefix));
}

Test(syslog_format, test_frame_checking)
{
  const gchar *data = "5 aaaaa";
  gsize data_length = strlen(data);

  LogMessage *msg = msg_format_construct_message(&parse_options, (const guchar *) data, data_length);

  gsize problem_position;
  cr_assert(syslog_format_handler(&parse_options, msg, (const guchar *) data, data_length, &problem_position));

  cr_assert(log_msg_is_tag_by_id(msg, LM_T_SYSLOG_UNEXPECTED_FRAMING));

  log_msg_unref(msg);
}

Test(syslog_format, test_framing_not_detected_when_frame_length_is_too_long)
{
  const gchar *data = "99999999999 a";
  gsize data_length = strlen(data);

  LogMessage *msg = msg_format_construct_message(&parse_options, (const guchar *) data, data_length);

  gsize problem_position;
  cr_assert(syslog_format_handler(&parse_options, msg, (const guchar *) data, data_length, &problem_position));

  cr_assert_not(log_msg_is_tag_by_id(msg, LM_T_SYSLOG_UNEXPECTED_FRAMING));

  log_msg_unref(msg);
}

Test(syslog_format, test_framing_not_detected_on_input_starting_with_whitespace)
{
  const gchar *data = " non-framed-whitespace";
  gsize data_length = strlen(data);

  LogMessage *msg = msg_format_construct_message(&parse_options, (const guchar *) data, data_length);

  gsize problem_position;
  cr_assert(syslog_format_handler(&parse_options, msg, (const guchar *) data, data_length, &problem_position));

  cr_assert_not(log_msg_is_tag_by_id(msg, LM_T_SYSLOG_UNEXPECTED_FRAMING));

  log_msg_unref(msg);
}
