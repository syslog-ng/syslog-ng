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
#include "rewrite/rewrite-set-level.h"
#include "logmsg/logmsg.h"
#include "grab-logging.h"

GlobalConfig *cfg = NULL;

static LogTemplate *
_create_template(const gchar *str)
{
  GError *error = NULL;
  LogTemplate *template = log_template_new(cfg, NULL);
  log_template_compile(template, str, &error);

  cr_expect_null(error);

  return template;
}

Test(set_level, numeric)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg = log_msg_new_empty();

  LogRewrite *set_level = log_rewrite_set_level_new(_create_template("1"), cfg);

  log_pipe_init(&set_level->super);
  log_msg_ref(msg);
  log_pipe_queue(&set_level->super, msg, &path_options);

  cr_assert_eq(msg->pri & LOG_PRIMASK, 1);

  log_msg_unref(msg);
  log_pipe_deinit(&set_level->super);
  log_pipe_unref(&set_level->super);
}

Test(set_level, large_number)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg = log_msg_new_empty();

  LogRewrite *set_level = log_rewrite_set_level_new(_create_template("8"), cfg);

  log_pipe_init(&set_level->super);
  log_pipe_queue(&set_level->super, msg, &path_options);

  assert_grabbed_log_contains("invalid level to set");

  log_pipe_deinit(&set_level->super);
  log_pipe_unref(&set_level->super);
}

static void
setup(void)
{
  app_startup();
  cfg = cfg_new_snippet();
  start_grabbing_messages();
}

static void
teardown(void)
{
  app_shutdown();
  stop_grabbing_messages();
  cfg_free(cfg);
}

TestSuite(set_level, .init = setup, .fini = teardown);


