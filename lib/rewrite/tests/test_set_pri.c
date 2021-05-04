/*
 * Copyright (c) 2021 Balazs Scheidler <bazsi77@gmail.com>
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Kokan <kokaipeter@gmail.com>
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

#include "apphook.h"
#include "rewrite/rewrite-set-pri.h"
#include "logmsg/logmsg.h"
#include "scratch-buffers.h"
#include "grab-logging.h"

GlobalConfig *cfg = NULL;
LogMessage *msg;

static LogTemplate *
_create_template(const gchar *str)
{
  GError *error = NULL;
  LogTemplate *template = log_template_new(cfg, NULL);
  cr_assert(log_template_compile(template, str, &error));

  cr_expect_null(error);

  return template;
}

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
_perform_set_pri(LogTemplate *template, LogMessage *msg_)
{
  LogRewrite *rewrite = log_rewrite_set_pri_new(template, cfg);
  log_template_unref(template);

  _perform_rewrite(rewrite, msg_);
}

static gboolean
_msg_pri_equals(LogMessage *msg_, gint pri)
{
  return msg_->pri == pri;
}

Test(set_pri, numeric)
{
  _perform_set_pri(_create_template("7"), msg);
  cr_assert(_msg_pri_equals(msg, LOG_KERN | LOG_DEBUG));

  _perform_set_pri(_create_template("189"), msg);
  cr_assert(_msg_pri_equals(msg, LOG_LOCAL7 | LOG_NOTICE));

  log_msg_set_value_by_name(msg, "pri_value", "137", -1);
  _perform_set_pri(_create_template("$pri_value"), msg);
  cr_assert(_msg_pri_equals(msg, 137));

  _perform_set_pri(_create_template("1023"), msg);
  cr_assert(_msg_pri_equals(msg, 127*8+7));

  _perform_set_pri(_create_template(" 123"), msg);
  cr_assert(_msg_pri_equals(msg, 123));

}

Test(set_pri, test_set_pri_with_various_invalid_values)
{
  debug_flag = 1;

  int default_pri = msg->pri;
  _perform_set_pri(_create_template("${nonexistentvalue}"), msg);
  assert_grabbed_log_contains("invalid value passed to set-pri()");
  cr_assert(_msg_pri_equals(msg, default_pri),
            "empty templates should not change the original pri");

  reset_grabbed_messages();
  _perform_set_pri(_create_template("1024"), msg);
  assert_grabbed_log_contains("invalid value passed to set-pri()");
  cr_assert(_msg_pri_equals(msg, default_pri),
            "too large numeric values should not change the original pri");

  reset_grabbed_messages();
  _perform_set_pri(_create_template("-1"), msg);
  assert_grabbed_log_contains("invalid value passed to set-pri()");
  cr_assert(_msg_pri_equals(msg, default_pri),
            "negative values should not change the original pri");

  reset_grabbed_messages();
  _perform_set_pri(_create_template("random-text"), msg);
  assert_grabbed_log_contains("invalid value passed to set-pri()");
  cr_assert(_msg_pri_equals(msg, default_pri),
            "non-numeric data should not change the original pri");

  reset_grabbed_messages();
  _perform_set_pri(_create_template("123 "), msg);
  assert_grabbed_log_contains("invalid value passed to set-pri()");
  cr_assert(_msg_pri_equals(msg, default_pri),
            "non-numeric data should not change the original pri");

  reset_grabbed_messages();
  _perform_set_pri(_create_template("123d"), msg);
  assert_grabbed_log_contains("invalid value passed to set-pri()");
  cr_assert(_msg_pri_equals(msg, default_pri),
            "non-numeric data should not change the original pri");
}

static void
setup(void)
{
  app_startup();
  cfg = cfg_new_snippet();
  start_grabbing_messages();
  msg = log_msg_new_empty();
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

TestSuite(set_pri, .init = setup, .fini = teardown);
