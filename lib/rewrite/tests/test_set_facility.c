/*
 * Copyright (c) 2020 Balazs Scheidler <bazsi77@gmail.com>
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

#include "syslog-names.h"
#include "apphook.h"
#include "rewrite/rewrite-set-facility.h"
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
_perform_set_facility(LogTemplate *template, LogMessage *msg_)
{
  LogRewrite *rewrite = log_rewrite_set_facility_new(template, cfg);
  log_template_unref(template);

  _perform_rewrite(rewrite, msg_);
}

static gboolean
_msg_facility_equals(LogMessage *msg_, gint fac)
{
  return (msg_->pri & LOG_FACMASK) == fac;
}

Test(set_facility, text)
{
  _perform_set_facility(_create_template("mail"), msg);
  cr_assert(_msg_facility_equals(msg, LOG_MAIL));

  _perform_set_facility(_create_template("news"), msg);
  cr_assert(_msg_facility_equals(msg, LOG_NEWS));

  _perform_set_facility(_create_template("kern"), msg);
  cr_assert(_msg_facility_equals(msg, LOG_KERN));
}

Test(set_facility, numeric)
{
  _perform_set_facility(_create_template("1"), msg);
  cr_assert(_msg_facility_equals(msg, FACILITY_CODE(1)));

  _perform_set_facility(_create_template("2"), msg);
  cr_assert(_msg_facility_equals(msg, FACILITY_CODE(2)));
}

Test(set_facility, test_set_facility_with_template_evaluating_to_empty_string)
{
  debug_flag = 1;

  gint default_facility = msg->pri & LOG_FACMASK;

  _perform_set_facility(_create_template("${nonexistentvalue}"), msg);

  cr_assert(_msg_facility_equals(msg, default_facility), "empty templates should not change the original facility");
  assert_grabbed_log_contains("invalid value passed to set-facility()");
}

Test(set_facility, large_number)
{
  debug_flag = 1;

  gint default_facility = msg->pri & LOG_FACMASK;

  _perform_set_facility(_create_template("128"), msg);

  cr_assert(_msg_facility_equals(msg, default_facility), "too large values should not change the original facility");
  assert_grabbed_log_contains("invalid value passed to set-facility()");
}

Test(set_facility, invalid)
{
  debug_flag = 1;

  gint default_facility = msg->pri & LOG_FACMASK;

  _perform_set_facility(_create_template("random-text"), msg);

  cr_assert(_msg_facility_equals(msg, default_facility), "too large values should not change the original facility");
  assert_grabbed_log_contains("invalid value passed to set-facility()");
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

TestSuite(set_facility, .init = setup, .fini = teardown);
