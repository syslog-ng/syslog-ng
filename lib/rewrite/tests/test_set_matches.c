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
