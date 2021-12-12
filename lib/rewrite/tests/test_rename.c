/*
 * Copyright (c) 2021 Balabit
 * Copyright (c) 2021 Kokan <kokaipeter@gmail.com>
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

#include "rewrite/rewrite-rename.h"
#include "logmsg/logmsg.h"
#include "scratch-buffers.h"
#include "apphook.h"

GlobalConfig *cfg = NULL;
LogMessage *msg;

static void
_perform_rewrite(LogRewrite *rewrite, LogMessage *msg_)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  cr_assert(log_pipe_init(&rewrite->super));
  log_pipe_queue(&rewrite->super, log_msg_ref(msg_), &path_options);
  log_pipe_deinit(&rewrite->super);
  log_pipe_unref(&rewrite->super);
}

static void
_perform_rename(const gchar *from, const char *to, LogMessage *msg_)
{
  LogRewrite *rename = log_rewrite_rename_new(cfg, log_msg_get_value_handle(from), log_msg_get_value_handle(to));

  _perform_rewrite(rename, msg_);
}

Test(rename, basic_rename)
{
  _perform_rename("MESSAGE", "m", msg);
  assert_log_message_value_by_name(msg, "m", "example");
  assert_log_message_value_unset_by_name(msg, "MESSAGE");
}

Test(rename, override_existing)
{
  _perform_rename(".SDATA.bar", "MESSAGE", msg);
  assert_log_message_value_by_name(msg, "MESSAGE", "foo");
  assert_log_message_value_unset_by_name(msg, ".SDATA.bar");
}

Test(rename, structued)
{
  _perform_rename(".SDATA.bar", ".json.bar", msg);
  assert_log_message_value_by_name(msg, ".json.bar", "foo");
  assert_log_message_value_unset_by_name(msg, ".SDATA.bar");
}

Test(rename, rename_empty)
{
  _perform_rename("empty", "really-empty", msg);
  assert_log_message_value_by_name(msg, "really-empty", "");
  assert_log_message_value_unset_by_name(msg, "empty");
}

Test(rename, source_destination_equals)
{
  _perform_rename("MESSAGE", "MESSAGE", msg);
  assert_log_message_value_by_name(msg, "MESSAGE", "example");
}

Test(rename, rename_not_existing_should_not_create_old_or_new)
{
  _perform_rename("should-not-exists", "something-else", msg);
  assert_log_message_value_unset_by_name(msg, "should-not-exists");
  assert_log_message_value_unset_by_name(msg, "something-else");
}

Test(rename, source_option_mandatory)
{
  LogRewrite *rename = log_rewrite_rename_new(cfg, 0, LM_V_MESSAGE);

  cr_assert_not(log_pipe_init(&rename->super));

  log_pipe_unref(&rename->super);
}

Test(rename, destination_option_mandatory)
{
  LogRewrite *rename = log_rewrite_rename_new(cfg, LM_V_MESSAGE, 0);

  cr_assert_not(log_pipe_init(&rename->super));

  log_pipe_unref(&rename->super);
}

static void
setup(void)
{
  app_startup();
  cfg = cfg_new_snippet();
  start_grabbing_messages();
  msg = log_msg_new_empty();

  log_msg_set_value(msg, LM_V_MESSAGE, "example", -1);
  log_msg_set_value_by_name(msg, ".SDATA.bar", "foo", -1);
  log_msg_set_value_by_name(msg, "empty", "", -1);
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  log_msg_unref(msg);
  stop_grabbing_messages();
  cfg_free(cfg);
  app_shutdown();
}

TestSuite(rename, .init = setup, .fini = teardown);
