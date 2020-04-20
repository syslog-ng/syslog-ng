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
#include <criterion/parameterized.h>
#include "tags-parser.h"
#include "apphook.h"
#include "msg_parse_lib.h"
#include "libtest/cr_template.h"

void
setup(void)
{
  configuration = cfg_new_snippet();
  app_startup();
}

void
teardown(void)
{
  app_shutdown();
  cfg_free(configuration);
}

TestSuite(tagsparser, .init = setup, .fini = teardown);

Test(tagsparser, input_list_is_transferred_to_a_list_of_tags)
{
  LogParser *tags_parser = tags_parser_new(configuration);

  log_pipe_init((LogPipe *)tags_parser);

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_MESSAGE, "foo,bar,baz", -1);
  log_msg_set_tag_by_name(msg, "tag-already-set");

  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  cr_assert(log_parser_process_message(tags_parser, &msg, &path_options));

  assert_log_message_has_tag(msg, "foo");
  assert_log_message_has_tag(msg, "bar");
  assert_log_message_has_tag(msg, "baz");
  assert_log_message_has_tag(msg, "tag-already-set");

  log_pipe_deinit((LogPipe *)tags_parser);
  log_pipe_unref((LogPipe *)tags_parser);
  log_msg_unref(msg);
}

Test(tagsparser, template_argument_uses_that_instead_of_MSG)
{
  LogParser *tags_parser = tags_parser_new(configuration);
  LogTemplate *template = compile_template("${PROGRAM}", FALSE);

  log_pipe_init((LogPipe *) tags_parser);

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_PROGRAM, "foo,bar,baz", -1);

  log_parser_set_template(tags_parser, template);
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  cr_assert(log_parser_process_message(tags_parser, &msg, &path_options));

  assert_log_message_has_tag(msg, "foo");
  assert_log_message_has_tag(msg, "bar");
  assert_log_message_has_tag(msg, "baz");

  log_pipe_deinit((LogPipe *)tags_parser);
  log_pipe_unref((LogPipe *)tags_parser);
  log_msg_unref(msg);
}
