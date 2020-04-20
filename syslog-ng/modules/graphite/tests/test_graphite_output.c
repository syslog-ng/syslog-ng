/*
 * Copyright (c) 2017 Balabit
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

#include <criterion/criterion.h>

#include "logmsg/logmsg.h"
#include "timeutils/misc.h"
#include "libtest/cr_template.h"
#include "apphook.h"
#include "cfg.h"

void
setup(void)
{
  app_startup();
  setenv("TZ", "UTC", TRUE);
  tzset();
  init_template_tests();
  cfg_load_module(configuration, "graphite");
}

void
teardown(void)
{
  deinit_template_tests();
  app_shutdown();
}

static void
_log_msg_set_recvd_time(LogMessage *msg, time_t t)
{
  msg->timestamps[LM_TS_RECVD].ut_sec = t;
  msg->timestamps[LM_TS_RECVD].ut_usec = 0;
  msg->timestamps[LM_TS_RECVD].ut_gmtoff = get_local_timezone_ofs(t);
}

TestSuite(graphite_output, .init = setup, .fini = teardown);

Test(graphite_output, test_graphite_plaintext_proto_simple)
{
  assert_template_format("$(graphite-output local.random.diceroll=4)", "local.random.diceroll 4 1139684315\n");
}

Test(graphite_output, test_graphite_output_hard_macro)
{
  LogMessage *msg = log_msg_new_empty();

  log_msg_set_value(msg, LM_V_MESSAGE, "árvíztűrőtükörfúrógép", -1);
  _log_msg_set_recvd_time(msg, 1139684315);

  assert_template_format_with_escaping_msg("$(graphite-output --key MESSAGE)", FALSE,
                                           "MESSAGE árvíztűrőtükörfúrógép 1139684315\n", msg);
  log_msg_unref(msg);
}

Test(graphite_output, test_graphite_output_key)
{
  assert_template_format("$(graphite-output --key APP.VALUE*)",
                         "APP.VALUE value 1139684315\n"
                         "APP.VALUE2 value 1139684315\n"
                         "APP.VALUE3 value 1139684315\n"
                         "APP.VALUE4 value 1139684315\n"
                         "APP.VALUE5 value 1139684315\n"
                         "APP.VALUE6 value 1139684315\n"
                         "APP.VALUE7 value 1139684315\n");
}

Test(graphite_output, test_graphite_output_custome_key_name)
{
  assert_template_format("$(graphite-output local.value=${APP.VALUE})", "local.value value 1139684315\n");
}

Test(graphite_output, test_graphite_output_timestamp)
{
  assert_template_format("$(graphite-output --timestamp 123 x=y)", "x y 123\n");
}
