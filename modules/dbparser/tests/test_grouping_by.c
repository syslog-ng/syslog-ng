/*
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Kokan <kokaipeter@gmail.com>
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
 *
 */

#include <criterion/criterion.h>
#include "libtest/cr_template.h"
#include "libtest/msg_parse_lib.h"

#include "groupingby.h"
#include "filter/filter-expr-parser.h"
#include "filter/filter-expr.h"
#include "apphook.h"
#include "cfg.h"
#include "scratch-buffers.h"

static LogTemplate *
_get_template(const gchar *template)
{
  LogTemplate *self = log_template_new(configuration, NULL);

  cr_assert(log_template_compile(self, template, NULL));

  return self;
}

static FilterExprNode *
_compile_filter_expr(gchar *expr)
{
  FilterExprNode *expr_node;

  CfgLexer *lexer = cfg_lexer_new_buffer(configuration, expr, strlen(expr));
  cr_assert(lexer != NULL);

  cr_assert(cfg_run_parser(configuration, lexer, &filter_expr_parser, (gpointer *) &expr_node, NULL));

  return expr_node;
}

static LogMessage *
_create_input_msg(const gchar *prog)
{
  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "key", "thesamekey", -1);
  log_msg_set_value_by_name(msg, "PROGRAM", prog, -1);
  return msg;
}

typedef struct _TestCapturePipe
{
  LogPipe super;
  LogMessage *captured_message;
} TestCapturePipe;

static void
test_capture_pipe_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  TestCapturePipe *self = (TestCapturePipe *) s;

  log_msg_unref(self->captured_message);
  self->captured_message = log_msg_ref(msg);
  log_pipe_forward_msg(s, msg, path_options);
}

TestCapturePipe *
test_capture_pipe_new(GlobalConfig *cfg)
{
  TestCapturePipe *self = g_new0(TestCapturePipe, 1);

  log_pipe_init_instance(&self->super, cfg);
  self->super.queue = test_capture_pipe_queue;
  return self;
}

Test(grouping_by, create_grouping_by)
{
  LogParser *parser = grouping_by_new(configuration);

  grouping_by_set_synthetic_message(parser, synthetic_message_new());

  grouping_by_set_timeout(parser, 1);

  LogTemplate *template = _get_template("$TEMPLATE");
  grouping_by_set_key_template(parser, template);
  log_template_unref(template);

  cr_assert(log_pipe_init(&parser->super));

  cr_assert(log_pipe_deinit(&parser->super));

  log_pipe_unref(&parser->super);
}

Test(grouping_by, grouping_by_aggregates_messages)
{
  TestCapturePipe *capture = test_capture_pipe_new(configuration);
  LogParser *parser = grouping_by_new(configuration);
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  LogTemplate *template = _get_template("$key");
  grouping_by_set_key_template(parser, template);
  log_template_unref(template);

  SyntheticMessage *sm = synthetic_message_new();
  cr_assert(synthetic_message_add_value_template_string(sm, configuration, "aggr", "$(list-slice :-1 $(context-values $PROGRAM))", NULL) == TRUE);

  grouping_by_set_synthetic_message(parser, sm);
  grouping_by_set_timeout(parser, 1);

  log_pipe_append(&parser->super, &capture->super);
  cr_assert(log_pipe_init(&capture->super) == TRUE);
  cr_assert(log_pipe_init(&parser->super) == TRUE);

  FilterExprNode *trigger = _compile_filter_expr("\"$(context-length)\" == \"3\"");
  grouping_by_set_trigger_condition(parser, trigger);

  LogMessage *msg = _create_input_msg("first");
  gboolean success = log_parser_process_message(parser, &msg, &path_options);
  cr_assert(success == TRUE);
  log_msg_unref(msg);

  msg = _create_input_msg("second");
  success = log_parser_process_message(parser, &msg, &path_options);
  cr_assert(success == TRUE);
  log_msg_unref(msg);

  msg = _create_input_msg("third");
  success = log_parser_process_message(parser, &msg, &path_options);
  cr_assert(success == TRUE);
  log_msg_unref(msg);

  assert_log_message_value_by_name(capture->captured_message, "aggr", "first,second,third");

  log_pipe_unref(&parser->super);
  log_pipe_unref(&capture->super);
}

Test(grouping_by, cfg_persist_name_not_equal)
{
  LogParser *parser = grouping_by_new(configuration);

  LogTemplate *template = _get_template("$TEMPLATE1");
  grouping_by_set_key_template(parser, template);
  log_template_unref(template);

  gchar *persist_name1 = g_strdup(log_pipe_get_persist_name(&parser->super));

  template = _get_template("$TEMPLATE2");
  grouping_by_set_key_template(parser, template);
  log_template_unref(template);

  gchar *persist_name2 = g_strdup(log_pipe_get_persist_name(&parser->super));

  cr_assert_str_neq(persist_name1, persist_name2);

  g_free(persist_name1);
  g_free(persist_name2);

  log_pipe_unref(&parser->super);
}

Test(grouping_by, cfg_persist_name_equal)
{
  LogParser *parser = grouping_by_new(configuration);

  LogTemplate *template = _get_template("$TEMPLATE1");
  grouping_by_set_key_template(parser, template);
  log_template_unref(template);

  gchar *persist_name1 = g_strdup(log_pipe_get_persist_name(&parser->super));

  template = _get_template("$TEMPLATE1");
  grouping_by_set_key_template(parser, template);
  log_template_unref(template);

  gchar *persist_name2 = g_strdup(log_pipe_get_persist_name(&parser->super));

  cr_assert_str_eq(persist_name1, persist_name2);

  g_free(persist_name1);
  g_free(persist_name2);

  log_pipe_unref(&parser->super);
}

static void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "basicfuncs");
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(grouping_by, .init = setup, .fini = teardown);
