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
#include "libtest/config_parse_lib.h"
#include "libtest/mock-logpipe.h"

#include "groupingby.h"
#include "filter/filter-expr-parser.h"
#include "filter/filter-expr.h"
#include "apphook.h"
#include "cfg.h"
#include "scratch-buffers.h"

static LogParser *
_compile_grouping_by(gchar *expr)
{
  LogParser *gby;
  cr_assert(parse_config(expr, LL_CONTEXT_PARSER, NULL, (gpointer *) &gby) == TRUE);
  return gby;
}

static LogMessage *
_create_input_msg(const gchar *prog)
{
  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "key", "thesamekey", -1);
  log_msg_set_value_by_name(msg, "PROGRAM", prog, -1);
  return msg;
}

void
_process_msg(LogParser *parser, const gchar *prog)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  LogMessage *msg = _create_input_msg(prog);
  /* NOTE: log_pipe_queue() consumes a reference */
  log_pipe_queue(&parser->super, msg, &path_options);
}


Test(grouping_by, grouping_by_produces_aggregate_as_the_trigger_is_received)
{
  LogPipeMock *capture = log_pipe_mock_new(configuration);
  LogParser *parser = _compile_grouping_by(
                        "grouping-by(key(\"$key\")"
                        "    aggregate("
                        "        value(\"aggr\" \"$(list-slice :-1 $(context-values $PROGRAM))\")"
                        "    )"
                        "    timeout(1)"
                        "    inject-mode(pass-through)"
                        "    trigger(\"$(context-length)\" == \"3\")"
                        ");");

  log_pipe_append(&parser->super, &capture->super);
  cr_assert(log_pipe_init(&capture->super) == TRUE);
  cr_assert(log_pipe_init(&parser->super) == TRUE);

  _process_msg(parser, "first");
  _process_msg(parser, "second");
  _process_msg(parser, "third");

  cr_assert(capture->captured_messages->len == 4);
  assert_log_message_value_by_name(log_pipe_mock_get_message(capture, 0), "PROGRAM", "first");
  assert_log_message_value_by_name(log_pipe_mock_get_message(capture, 1), "PROGRAM", "second");
  /* the aggregate comes before the triggering message */
  assert_log_message_value_by_name(log_pipe_mock_get_message(capture, 2), "aggr", "first,second,third");
  assert_log_message_value_by_name(log_pipe_mock_get_message(capture, 3), "PROGRAM", "third");

  log_pipe_unref(&parser->super);
  log_pipe_unref(&capture->super);
}

Test(grouping_by, grouping_by_adds_prefix_to_name_value_pairs_if_specified)
{
  LogPipeMock *capture = log_pipe_mock_new(configuration);
  LogParser *parser = _compile_grouping_by(
                        "grouping-by(key(\"$key\")"
                        "    aggregate("
                        "        value(\"aggr\" \"$(list-slice :-1 $(context-values $PROGRAM))\")"
                        "    )"
                        "    timeout(1)"
                        "    inject-mode(aggregate-only)"
                        "    prefix('prefix.')"
                        "    trigger(\"$(context-length)\" == \"3\")"
                        ");");

  log_pipe_append(&parser->super, &capture->super);
  cr_assert(log_pipe_init(&capture->super) == TRUE);
  cr_assert(log_pipe_init(&parser->super) == TRUE);

  _process_msg(parser, "first");
  _process_msg(parser, "second");
  _process_msg(parser, "third");

  cr_assert(capture->captured_messages->len == 1);
  /* the aggregate comes before the triggering message */
  assert_log_message_value_by_name(log_pipe_mock_get_message(capture, 0), "prefix.aggr", "first,second,third");

  log_pipe_unref(&parser->super);
  log_pipe_unref(&capture->super);
}

Test(grouping_by, grouping_by_drops_original_messages_if_inject_mode_is_aggregate_only)
{
  LogPipeMock *capture = log_pipe_mock_new(configuration);
  LogParser *parser = _compile_grouping_by(
                        "grouping-by(key(\"$key\")"
                        "    aggregate("
                        "        value(\"aggr\" \"$(list-slice :-1 $(context-values $PROGRAM))\")"
                        "    )"
                        "    timeout(1)"
                        "    inject-mode(aggregate-only)"
                        "    trigger(\"$(context-length)\" == \"3\")"
                        ");");

  log_pipe_append(&parser->super, &capture->super);
  cr_assert(log_pipe_init(&capture->super) == TRUE);
  cr_assert(log_pipe_init(&parser->super) == TRUE);

  _process_msg(parser, "first");
  _process_msg(parser, "second");
  _process_msg(parser, "third");

  cr_assert(capture->captured_messages->len == 1);
  assert_log_message_value_by_name(log_pipe_mock_get_message(capture, 0), "aggr", "first,second,third");

  log_pipe_unref(&parser->super);
  log_pipe_unref(&capture->super);
}

Test(grouping_by, cfg_persist_name_not_equal)
{
  LogParser *parser = _compile_grouping_by("grouping-by(key(\"$TEMPLATE1\"));");

  gchar *persist_name1 = g_strdup(log_pipe_get_persist_name(&parser->super));
  log_pipe_unref(&parser->super);

  parser = _compile_grouping_by("grouping-by(key(\"$TEMPLATE2\"));");
  gchar *persist_name2 = g_strdup(log_pipe_get_persist_name(&parser->super));
  log_pipe_unref(&parser->super);

  cr_assert_str_neq(persist_name1, persist_name2);

  g_free(persist_name1);
  g_free(persist_name2);
}

Test(grouping_by, cfg_persist_name_equal)
{
  LogParser *parser = _compile_grouping_by("grouping-by(key(\"$TEMPLATE1\"));");
  gchar *persist_name1 = g_strdup(log_pipe_get_persist_name(&parser->super));
  log_pipe_unref(&parser->super);

  parser = _compile_grouping_by("grouping-by(key(\"$TEMPLATE1\"));");
  gchar *persist_name2 = g_strdup(log_pipe_get_persist_name(&parser->super));
  log_pipe_unref(&parser->super);

  cr_assert_str_eq(persist_name1, persist_name2);

  g_free(persist_name1);
  g_free(persist_name2);
}

static void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "basicfuncs");
  cfg_load_module(configuration, "correlation");
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(grouping_by, .init = setup, .fini = teardown);
