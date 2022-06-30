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
#include "libtest/msg_parse_lib.h"

#include "apphook.h"
#include "rewrite/rewrite-set-tag.h"
#include "logmsg/logmsg.h"
#include "scratch-buffers.h"

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
_perform_set_tag(LogTemplate *template, gboolean onoff, LogMessage *msg_)
{
  LogRewrite *rewrite = log_rewrite_set_tag_new(template, onoff, cfg);
  log_template_unref(template);

  _perform_rewrite(rewrite, msg_);
}

Test(set_tag, literal_tags)
{
  _perform_set_tag(_create_template("literal-tag"), TRUE, msg);
  assert_log_message_has_tag(msg, "literal-tag");

  _perform_set_tag(_create_template("literal-tag"), FALSE, msg);
  assert_log_message_doesnt_have_tag(msg, "literal-tag");
}

Test(set_tag, templates_in_set_tag)
{
  log_msg_set_value_by_name(msg, "tag_name", "FOOBAR", -1);
  _perform_set_tag(_create_template("tag-${tag_name}"), TRUE, msg);

  log_msg_set_value_by_name(msg, "tag_name", "BARFOO", -1);
  _perform_set_tag(_create_template("tag-${tag_name}"), TRUE, msg);
  assert_log_message_has_tag(msg, "tag-FOOBAR");
  assert_log_message_has_tag(msg, "tag-BARFOO");

  _perform_set_tag(_create_template("tag-${tag_name}"), FALSE, msg);
  assert_log_message_doesnt_have_tag(msg, "tag-BARFOO");
}

Test(set_tag, clone_template)
{
  LogTemplate *template = log_template_new(cfg, "dummy");
  cr_assert(log_template_compile(template, "not literal $MESSAGE", NULL));
  LogRewrite *rewrite = log_rewrite_set_tag_new(template, true, cfg);

  LogRewrite *clone = (LogRewrite *)log_pipe_clone(&rewrite->super);

  cr_assert(log_pipe_init(&rewrite->super));
  cr_assert(log_pipe_init(&clone->super));

  cr_assert(log_pipe_deinit(&rewrite->super));
  cr_assert(log_pipe_deinit(&clone->super));

  log_pipe_unref(&rewrite->super);
  log_pipe_unref(&clone->super);
}

Test(set_tag, clone_tag_id)
{
  LogTemplate *template = log_template_new(cfg, "dummy");
  cr_assert(log_template_compile(template, "syslog", NULL));
  LogRewrite *rewrite = log_rewrite_set_tag_new(template, true, cfg);

  LogRewrite *clone = (LogRewrite *)log_pipe_clone(&rewrite->super);

  cr_assert(log_pipe_init(&rewrite->super));
  cr_assert(log_pipe_init(&clone->super));

  cr_assert(log_pipe_deinit(&rewrite->super));
  cr_assert(log_pipe_deinit(&clone->super));

  log_pipe_unref(&rewrite->super);
  log_pipe_unref(&clone->super);
}

static void
setup(void)
{
  app_startup();
  cfg = cfg_new_snippet();
  msg = log_msg_new_empty();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  log_msg_unref(msg);
  cfg_free(cfg);
  app_shutdown();
}

TestSuite(set_tag, .init = setup, .fini = teardown);
