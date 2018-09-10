/*
 * Copyright (c) 2015 Balabit
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
 */
#include "testutils.h"
#include "kv-parser.h"
#include "apphook.h"
#include "msg_parse_lib.h"

#define kv_parser_testcase_begin(func, args)             \
  do                                                            \
    {                                                           \
      testcase_begin("%s(%s)", func, args);                     \
      kv_parser =                                               \
        kv_parser_new(NULL);        \
      log_pipe_init((LogPipe*)kv_parser);                 \
    }                                                           \
  while (0)

#define kv_parser_testcase_end()                           \
  do                                                            \
    {                                                           \
      log_pipe_deinit((LogPipe*)kv_parser);                     \
      log_pipe_unref(&kv_parser->super);                  \
      testcase_end();                                           \
    }                                                           \
  while (0)

#define KV_PARSER_TESTCASE(x, ...) \
  do {                                                          \
      kv_parser_testcase_begin(#x, #__VA_ARGS__);     \
      x(__VA_ARGS__);                                           \
      kv_parser_testcase_end();                               \
  } while(0)

LogParser *kv_parser;

static LogMessage *
parse_kv_into_log_message_no_check(const gchar *kv)
{
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogParser *cloned_parser;

  cloned_parser = (LogParser *) log_pipe_clone(&kv_parser->super);
  msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_MESSAGE, kv, -1);
  if (!log_parser_process_message(cloned_parser, &msg, &path_options))
    {
      log_msg_unref(msg);
      log_pipe_unref(&cloned_parser->super);
      return NULL;
    }
  log_pipe_unref(&cloned_parser->super);
  return msg;
}

static LogMessage *
parse_kv_into_log_message(const gchar *kv)
{
  LogMessage *msg;

  msg = parse_kv_into_log_message_no_check(kv);
  assert_not_null(msg, "expected kv-parser success and it returned failure, kv=%s", kv);
  return msg;
}

static void
test_kv_parser_basics(void)
{
  LogMessage *msg;

  msg = parse_kv_into_log_message("foo=bar");
  assert_log_message_value(msg, log_msg_get_value_handle("foo"), "bar");
  log_msg_unref(msg);

  kv_parser_set_prefix(kv_parser, ".prefix.");
  msg = parse_kv_into_log_message("foo=bar");
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.foo"), "bar");
  log_msg_unref(msg);
}

static void
test_kv_parser_uses_template_to_parse_input(void)
{
  LogMessage *msg;
  LogTemplate *template;

  template = log_template_new(NULL, NULL);
  log_template_compile(template, "foo=bar", NULL);
  log_parser_set_template(kv_parser, template);
  msg = parse_kv_into_log_message("foo=this-value-doesnot-matter-as-template-overrides");
  assert_log_message_value(msg, log_msg_get_value_handle("foo"), "bar");
  log_msg_unref(msg);
}

static void
test_kv_parser_audit(void)
{
  LogMessage *msg;

  msg = parse_kv_into_log_message("type=EXECVE msg=audit(1436899154.146:186135): argc=6 a0=\"modprobe\" a1=\"--set-version=3.19.0-22-generic\" a2=\"--ignore-install\" a3=\"--quiet\" a4=\"--show-depends\" a5=\"sata_sis\"");
  assert_log_message_value_by_name(msg, "type", "EXECVE");
  assert_log_message_value_by_name(msg, "msg", "audit(1436899154.146:186135):");
  assert_log_message_value_by_name(msg, "argc", "6");
  assert_log_message_value_by_name(msg, "a0", "modprobe");
  assert_log_message_value_by_name(msg, "a1", "--set-version=3.19.0-22-generic");
  assert_log_message_value_by_name(msg, "a2", "--ignore-install");
  assert_log_message_value_by_name(msg, "a3", "--quiet");
  assert_log_message_value_by_name(msg, "a4", "--show-depends");
  assert_log_message_value_by_name(msg, "a5", "sata_sis");
  log_msg_unref(msg);

  msg = parse_kv_into_log_message("type=LOGIN msg=audit(1437419821.034:2972): pid=4160 uid=0 auid=0 ses=221 msg='op=PAM:session_close acct=\"root\" exe=\"/usr/sbin/cron\" hostname=? addr=? terminal=cron res=success'");
  assert_log_message_value_by_name(msg, "type", "LOGIN");
  /*  assert_log_message_value_by_name(msg, "msg", "audit(1437419821.034:2972):"); */
  assert_log_message_value_by_name(msg, "pid", "4160");
  assert_log_message_value_by_name(msg, "uid", "0");
  assert_log_message_value_by_name(msg, "auid", "0");
  assert_log_message_value_by_name(msg, "ses", "221");
  assert_log_message_value_by_name(msg, "msg",
                                   "op=PAM:session_close acct=\"root\" exe=\"/usr/sbin/cron\" hostname=? addr=? terminal=cron res=success");
  log_msg_unref(msg);
}

static void
test_kv_parser_extract_stray_words(void)
{
  LogMessage *msg;

  kv_parser_set_stray_words_value_name(kv_parser, "stray");
  kv_parser_set_prefix(kv_parser, ".junos.");
  kv_parser_set_pair_separator(kv_parser, ";");
  log_pipe_deinit((LogPipe *)kv_parser);
  log_pipe_init((LogPipe *)kv_parser);
  msg = parse_kv_into_log_message("VSYS=public; Slot=5/1; protocol=17; source-ip=10.116.214.221; source-port=50989; "
                                  "destination-ip=172.16.236.16; destination-port=162;time=2016/02/18 16:00:07; "
                                  "interzone-emtn_s1_vpn-enodeb_om inbound; policy=370;");
  assert_log_message_value_by_name(msg, ".junos.VSYS", "public");
  assert_log_message_value_by_name(msg, ".junos.Slot", "5/1");
  assert_log_message_value_by_name(msg, ".junos.protocol", "17");
  assert_log_message_value_by_name(msg, ".junos.source-ip", "10.116.214.221");
  assert_log_message_value_by_name(msg, ".junos.source-port", "50989");
  assert_log_message_value_by_name(msg, ".junos.destination-ip", "172.16.236.16");
  assert_log_message_value_by_name(msg, ".junos.destination-port", "162");
  assert_log_message_value_by_name(msg, ".junos.time", "2016/02/18 16:00:07");
  assert_log_message_value_by_name(msg, ".junos.policy", "370");
  assert_log_message_value_by_name(msg, "stray", "\"interzone-emtn_s1_vpn-enodeb_om inbound;\"");
  log_msg_unref(msg);

  kv_parser_set_prefix(kv_parser, ".prefix.");
  kv_parser_set_pair_separator(kv_parser, " ");
  kv_parser_set_stray_words_value_name(kv_parser, ".stray");
  msg = parse_kv_into_log_message("foo='qu'x asd");
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.foo"), "'qu'");
  assert_log_message_value(msg, log_msg_get_value_handle(".stray"), "asd");
  log_msg_unref(msg);
}

static void
test_kv_parser(void)
{
  KV_PARSER_TESTCASE(test_kv_parser_basics);
  KV_PARSER_TESTCASE(test_kv_parser_audit);
  KV_PARSER_TESTCASE(test_kv_parser_uses_template_to_parse_input);
  KV_PARSER_TESTCASE(test_kv_parser_extract_stray_words);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();

  test_kv_parser();
  app_shutdown();
  return 0;
}
