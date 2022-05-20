/*
 * Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
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
#include "libtest/grab-logging.h"
#include "libtest/msg_parse_lib.h"
#include "libtest/cr_template.h"

#include "apphook.h"
#include "rewrite/rewrite-set-matches.h"
#include "rewrite/rewrite-unset-matches.h"
#include "logmsg/logmsg.h"
#include "scratch-buffers.h"

GlobalConfig *configuration = NULL;
LogMessage *msg;

static void
_perform_rewrite(LogRewrite *rewrite, LogMessage *msg_)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  log_pipe_init(&rewrite->super);
  log_pipe_queue(&rewrite->super, log_msg_ref(msg_), &path_options);
  log_pipe_deinit(&rewrite->super);
  log_pipe_unref(&rewrite->super);
}

static void
_perform_set_matches(LogTemplate *template, LogMessage *msg_)
{
  LogRewrite *rewrite = log_rewrite_set_matches_new(template, configuration);
  log_template_unref(template);

  _perform_rewrite(rewrite, msg_);
}

static void
_perform_unset_matches(LogMessage *msg_)
{
  LogRewrite *rewrite = log_rewrite_unset_matches_new(configuration);

  _perform_rewrite(rewrite, msg_);
}

Test(set_matches, numeric)
{
  log_msg_set_match(msg, 0, "whatever", -1);
  _perform_set_matches(compile_template("foo,bar"), msg);
  assert_log_message_value_unset_by_name(msg, "0");
  assert_log_message_match_value(msg, 1, "foo");
  assert_log_message_match_value(msg, 2, "bar");
}

Test(set_matches, too_many_items)
{
  cr_assert(LOGMSG_MAX_MATCHES == 256);

  log_msg_set_match(msg, 0, "whatever", -1);
  _perform_set_matches(
    compile_template("1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,"
                     "16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,"
                     "32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,"
                     "48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,"
                     "64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,"
                     "80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,"
                     "96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,"
                     "112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,"
                     "128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,"
                     "144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,"
                     "160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,"
                     "176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,"
                     "192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,"
                     "208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,"
                     "224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,"
                     "240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,thisisignored"),
    msg);
  assert_log_message_value_unset_by_name(msg, "0");

  for (gint i = 1; i < 256; i++)
    {
      gchar buf[16];

      g_snprintf(buf, sizeof(buf), "%d", i);
      assert_log_message_match_value(msg, i, buf);
    }
  /* the last item is ignored */
  assert_log_message_match_value(msg, 256, "");
  assert_log_message_value_by_name(msg, "256", "");
}

Test(set_matches, unset_matches)
{
  log_msg_set_match(msg, 0, "whatever", -1);
  log_msg_set_match(msg, 1, "foo", -1);
  log_msg_set_match(msg, 2, "bar", -1);
  _perform_unset_matches(msg);
  assert_log_message_value_unset_by_name(msg, "0");
  assert_log_message_value_unset_by_name(msg, "1");
  assert_log_message_value_unset_by_name(msg, "2");
}

static void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
  start_grabbing_messages();
  msg = log_msg_new_empty();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  log_msg_unref(msg);
  stop_grabbing_messages();
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(set_matches, .init = setup, .fini = teardown);
