/*
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
#include "rewrite/rewrite-set-severity.h"
#include "logmsg/logmsg.h"
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
_perform_set_severity(LogTemplate *template, LogMessage *msg_)
{
  LogRewrite *rewrite = log_rewrite_set_severity_new(template, cfg);
  log_template_unref(template);

  _perform_rewrite(rewrite, msg_);
}

static gboolean
_msg_severity_equals(LogMessage *msg_, gint sev)
{
  return (msg_->pri & LOG_PRIMASK) == sev;
}

Test(set_severity, text)
{
  _perform_set_severity(_create_template("error"), msg);
  cr_assert(_msg_severity_equals(msg, LOG_ERR));

  _perform_set_severity(_create_template("crit"), msg);
  cr_assert(_msg_severity_equals(msg, LOG_CRIT));

  _perform_set_severity(_create_template("debug"), msg);
  cr_assert(_msg_severity_equals(msg, LOG_DEBUG));
}

Test(set_severity, numeric)
{
  _perform_set_severity(_create_template("1"), msg);
  cr_assert(_msg_severity_equals(msg, 1));
}

Test(set_severity, test_set_severity_with_various_invalid_values)
{
  debug_flag = 1;

  int default_severity = msg->pri & LOG_PRIMASK;
  _perform_set_severity(_create_template("${nonexistentvalue}"), msg);
  assert_grabbed_log_contains("invalid value passed to set-severity()");
  cr_assert(_msg_severity_equals(msg, default_severity),
            "empty templates should not change the original severity");

  reset_grabbed_messages();
  _perform_set_severity(_create_template("8"), msg);
  assert_grabbed_log_contains("invalid value passed to set-severity()");
  cr_assert(_msg_severity_equals(msg, default_severity),
            "too large numeric values should not change the original severity");

  reset_grabbed_messages();
  _perform_set_severity(_create_template("-1"), msg);
  assert_grabbed_log_contains("invalid value passed to set-severity()");
  cr_assert(_msg_severity_equals(msg, default_severity),
            "negative values should not change the original severity");

  reset_grabbed_messages();
  _perform_set_severity(_create_template("random-text"), msg);
  assert_grabbed_log_contains("invalid value passed to set-severity()");
  cr_assert(_msg_severity_equals(msg, default_severity),
            "non-numeric data should not change the original severity");
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
  log_msg_unref(msg);
  stop_grabbing_messages();
  cfg_free(cfg);
  app_shutdown();
}

TestSuite(set_severity, .init = setup, .fini = teardown);
