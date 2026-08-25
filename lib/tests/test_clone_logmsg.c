/*
 * Copyright (c) 2016 Balabit
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
#include "libtest/msg_parse_lib.h"

#include "logmsg/logmsg.h"
#include "serialize.h"
#include "apphook.h"
#include "gsockaddr.h"
#include "logpipe.h"
#include "cfg.h"
#include "plugin.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

MsgFormatOptions parse_options;

void
assert_new_log_message_attributes(LogMessage *log_message)
{
  assert_log_message_value(log_message, LM_V_HOST, "newhost");
  assert_log_message_value(log_message, LM_V_HOST_FROM, "newhost");
  assert_log_message_value(log_message, LM_V_MESSAGE, "newmsg");
  assert_log_message_value(log_message, LM_V_PROGRAM, "newprogram");
  assert_log_message_value(log_message, LM_V_PID, "newpid");
  assert_log_message_value(log_message, LM_V_MSGID, "newmsgid");
  assert_log_message_value(log_message, LM_V_SOURCE, "newsource");
  assert_log_message_value(log_message, log_msg_get_value_handle("newvalue"), "newvalue");
}

void
set_new_log_message_attributes(LogMessage *log_message)
{
  log_msg_set_value(log_message, LM_V_HOST, "newhost", -1);
  log_msg_set_value(log_message, LM_V_HOST_FROM, "newhost", -1);
  log_msg_set_value(log_message, LM_V_MESSAGE, "newmsg", -1);
  log_msg_set_value(log_message, LM_V_PROGRAM, "newprogram", -1);
  log_msg_set_value(log_message, LM_V_PID, "newpid", -1);
  log_msg_set_value(log_message, LM_V_MSGID, "newmsgid", -1);
  log_msg_set_value(log_message, LM_V_SOURCE, "newsource", -1);
  log_msg_set_value_by_name(log_message, "newvalue", "newvalue", -1);
}


void
setup(void)
{
  app_startup();
  init_parse_options_and_load_syslogformat(&parse_options);
}

void
teardown(void)
{
  deinit_syslogformat_module();
  app_shutdown();
}

TestSuite(clone_logmsg, .init = setup, .fini = teardown);

/*
 * Criterion parameter payloads must be self-contained here.
 * We use fixed-size arrays (not pointers) to avoid pointer invalidation across
 * worker process boundaries on macOS.
 * The wrapper struct is required because Criterion parameter arrays are
 * struct-based, and we need a by-value message buffer in each test case.
 */
typedef struct _CloneLogMessageParam
{
  gchar msg[2048];
} CloneLogMessageParam;

ParameterizedTestParameters(clone_logmsg, test_cloning_with_log_message)
{
  static CloneLogMessageParam messages[] =
  {
    {"<7>1 2006-10-29T01:59:59.156+01:00 mymachine.example.com evntslog - ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry..."},
    {"<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [exampleSDID@0 iut=\"3\"] [eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry..."},
  };

  return cr_make_param_array(CloneLogMessageParam, messages, G_N_ELEMENTS(messages));
}

ParameterizedTest(CloneLogMessageParam *param, clone_logmsg, test_cloning_with_log_message)
{
  LogMessage *original_log_message, *log_message, *cloned_log_message;
  regex_t bad_hostname;
  GSockAddr *addr = g_sockaddr_inet_new("10.10.10.10", 1010);
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  parse_options.flags = LP_SYSLOG_PROTOCOL;
  parse_options.bad_hostname = &bad_hostname;

  original_log_message = msg_format_parse(&parse_options, (const guchar *) param->msg, strlen(param->msg));
  log_msg_set_saddr(original_log_message, addr);
  log_message = msg_format_parse(&parse_options, (const guchar *) param->msg, strlen(param->msg));
  log_msg_set_saddr(log_message, addr);
  log_message->timestamps[LM_TS_STAMP] = original_log_message->timestamps[LM_TS_STAMP];

  log_msg_set_tag_by_name(log_message, "newtag");
  path_options.ack_needed = FALSE;

  cloned_log_message = log_msg_clone_cow(log_message, &path_options);
  assert_log_messages_equal(cloned_log_message, original_log_message);

  set_new_log_message_attributes(cloned_log_message);

  assert_log_messages_equal(log_message, original_log_message);
  assert_new_log_message_attributes(cloned_log_message);
  assert_log_message_has_tag(cloned_log_message, "newtag");

  g_sockaddr_unref(addr);
  log_msg_unref(cloned_log_message);
  log_msg_unref(log_message);
  log_msg_unref(original_log_message);
}

Test(clone_logmsg, test_unset_value_accounts_for_newly_owned_payload_like_set_value_does)
{
  const gchar *msg_str =
    "<7>1 2006-10-29T01:59:59.156+01:00 mymachine.example.com evntslog - ID47 "
    "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"] BOMAn application event log entry...";
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *original, *clone_for_unset, *clone_for_set;
  NVHandle handle;

  parse_options.flags = LP_SYSLOG_PROTOCOL;
  path_options.ack_needed = FALSE;

  handle = log_msg_get_value_handle(".SDATA.exampleSDID@0.iut");

  original = msg_format_parse(&parse_options, (const guchar *) msg_str, strlen(msg_str));
  clone_for_unset = log_msg_clone_cow(original, &path_options);
  cr_assert_not((clone_for_unset->flags & LF_STATE_OWN_PAYLOAD),
                "test precondition: a fresh clone must not yet own its payload");

  gsize allocated_bytes_before_unset = clone_for_unset->allocated_bytes;
  log_msg_unset_value(clone_for_unset, handle);
  cr_assert((clone_for_unset->flags & LF_STATE_OWN_PAYLOAD),
            "test precondition: unset_value() must have taken payload ownership");

  cr_assert_gt(clone_for_unset->allocated_bytes, allocated_bytes_before_unset,
               "log_msg_unset_value() must add the newly-owned payload's size to allocated_bytes");

  log_msg_unref(clone_for_unset);
  log_msg_unref(original);

  original = msg_format_parse(&parse_options, (const guchar *) msg_str, strlen(msg_str));
  clone_for_set = log_msg_clone_cow(original, &path_options);
  cr_assert_not((clone_for_set->flags & LF_STATE_OWN_PAYLOAD),
                "test precondition: a fresh clone must not yet own its payload");

  gsize allocated_bytes_before_set = clone_for_set->allocated_bytes;
  log_msg_set_value(clone_for_set, handle, "5", -1);
  cr_assert((clone_for_set->flags & LF_STATE_OWN_PAYLOAD),
            "test precondition: set_value() must have taken payload ownership");

  cr_assert_gt(clone_for_set->allocated_bytes, allocated_bytes_before_set,
               "sanity check: set_value()'s ownership-transition accounting must work");

  log_msg_unref(clone_for_set);
  log_msg_unref(original);
}

Test(clone_logmsg, test_set_value_indirect_accounts_for_newly_owned_payload_like_set_value_does)
{
  const gchar *msg_str =
    "<7>1 2006-10-29T01:59:59.156+01:00 mymachine.example.com evntslog - ID47 "
    "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"] BOMAn application event log entry...";
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *original, *clone;
  NVHandle ref_handle, indirect_handle;

  parse_options.flags = LP_SYSLOG_PROTOCOL;
  path_options.ack_needed = FALSE;

  ref_handle = LM_V_MESSAGE;
  indirect_handle = log_msg_get_value_handle("newindirectvalue");

  original = msg_format_parse(&parse_options, (const guchar *) msg_str, strlen(msg_str));
  clone = log_msg_clone_cow(original, &path_options);
  cr_assert_not((clone->flags & LF_STATE_OWN_PAYLOAD),
                "test precondition: a fresh clone must not yet own its payload");

  gsize allocated_bytes_before = clone->allocated_bytes;
  log_msg_set_value_indirect(clone, indirect_handle, ref_handle, 0, 3);
  cr_assert((clone->flags & LF_STATE_OWN_PAYLOAD),
            "test precondition: set_value_indirect() must have taken payload ownership");

  cr_assert_gt(clone->allocated_bytes, allocated_bytes_before,
               "log_msg_set_value_indirect_with_type() must add the newly-owned payload's size to allocated_bytes");

  log_msg_unref(clone);
  log_msg_unref(original);
}
  log_msg_unref(original);
}
