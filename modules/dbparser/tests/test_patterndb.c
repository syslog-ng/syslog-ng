/*
 * Copyright (c) 2010-2018 Balabit
 * Copyright (c) 2010-2015 Balázs Scheidler <balazs.scheidler@balabit.com>
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

#include "apphook.h"
#include "logmsg/logmsg.h"
#include "messages.h"
#include "filter/filter-expr.h"
#include "patterndb.h"
#include "pdb-file.h"
#include "plugin.h"
#include "cfg.h"
#include "timerwheel.h"
#include "libtest/msg_parse_lib.h"
#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib/gstdio.h>

#include "test_patterndb.h"

GPtrArray *messages;
gboolean keep_patterndb_state = FALSE;

static void
_emit_func(LogMessage *msg, gboolean synthetic, gpointer user_data)
{
  g_ptr_array_add(messages, log_msg_ref(msg));
}

static void
assert_pdb_file_valid(const gchar *filename_)
{
  GError *error = NULL;
  gboolean success;

  success = pdb_file_validate_in_tests(filename_, &error);
  cr_assert(success, "Error validating patterndb, error=%s\n", error ? error->message : "unknown");
  g_clear_error(&error);
}

static PatternDB *
_create_pattern_db(const gchar *pdb, gchar **filename)
{
  PatternDB *patterndb = pattern_db_new();
  messages = g_ptr_array_new();

  pattern_db_set_emit_func(patterndb, _emit_func, NULL);

  g_file_open_tmp("patterndbXXXXXX.xml", filename, NULL);
  g_file_set_contents(*filename, pdb, strlen(pdb), NULL);

  assert_pdb_file_valid(*filename);

  cr_assert(pattern_db_reload_ruleset(patterndb, configuration, *filename), "Error loading ruleset [[[%s]]]",
            *filename);
  cr_assert_str_eq(pattern_db_get_ruleset_pub_date(patterndb), "2010-02-22", "Invalid pubdate");

  return patterndb;
}

static void
_destroy_pattern_db(PatternDB *patterndb, gchar *filename)
{
  if (messages)
    {
      g_ptr_array_foreach(messages, (GFunc) log_msg_unref, NULL);
      g_ptr_array_free(messages, TRUE);
    }
  messages = NULL;
  pattern_db_free(patterndb);
  g_unlink(filename);
}

static void
_reset_pattern_db_state(PatternDB *patterndb)
{
  pattern_db_forget_state(patterndb);
  g_ptr_array_foreach(messages, (GFunc) log_msg_unref, NULL);
  g_ptr_array_set_size(messages, 0);
}


static gboolean
_process(PatternDB *patterndb, LogMessage *msg)
{
  if (!keep_patterndb_state)
    _reset_pattern_db_state(patterndb);
  keep_patterndb_state = FALSE;
  return pattern_db_process(patterndb, msg);
}

static void
_dont_reset_patterndb_state_for_the_next_call(void)
{
  keep_patterndb_state = TRUE;
}

static void
_advance_time(PatternDB *patterndb, gint timeout)
{
  if (timeout)
    pattern_db_advance_time(patterndb, timeout + 1);
}

static LogMessage *
_get_output_message(gint ndx)
{
  cr_assert(ndx < messages->len, "Expected the %d. message, but no such message was returned by patterndb\n", ndx);
  return (LogMessage *) g_ptr_array_index(messages, ndx);
}

static LogMessage *
_construct_message_with_nvpair(const gchar *program, const gchar *message, const gchar *name, const gchar *value)
{
  LogMessage *msg = log_msg_new_empty();

  log_msg_set_value(msg, LM_V_MESSAGE, message, strlen(message));
  log_msg_set_value(msg, LM_V_PROGRAM, program, strlen(program));
  log_msg_set_value(msg, LM_V_HOST, MYHOST, strlen(MYHOST));
  log_msg_set_value(msg, LM_V_PID, MYPID, strlen(MYPID));
  if (name)
    log_msg_set_value_by_name(msg, name, value, -1);
  msg->timestamps[LM_TS_STAMP].ut_sec = msg->timestamps[LM_TS_RECVD].ut_sec;

  return msg;
}

static LogMessage *
_construct_message(const gchar *program, const gchar *message)
{
  return _construct_message_with_nvpair(program, message, NULL, NULL);
}

static void
_feed_message_to_correllation_state(PatternDB *patterndb, const gchar *program, const gchar *message, const gchar *name,
                                    const gchar *value)
{
  LogMessage *msg;
  gboolean result;

  msg = _construct_message_with_nvpair(program, message, name, value);
  result = _process(patterndb, msg);
  log_msg_unref(msg);
  cr_assert(result, "patterndb expected to match but it didn't");
  _dont_reset_patterndb_state_for_the_next_call();
}

static void
assert_msg_with_program_matches_and_nvpair_equals(PatternDB *patterndb, const gchar *program, const gchar *message,
                                                  const gchar *name, const gchar *expected_value)
{
  LogMessage *msg;
  gboolean result;

  msg = _construct_message(program, message);
  result = _process(patterndb, msg);
  cr_assert(result, "patterndb expected to match but it didn't");
  assert_log_message_value(msg, log_msg_get_value_handle(name), expected_value);
  log_msg_unref(msg);
}

static void
assert_msg_matches_and_nvpair_equals(PatternDB *patterndb, const gchar *pattern, const gchar *name, const gchar *value)
{
  assert_msg_with_program_matches_and_nvpair_equals(patterndb, "prog1", pattern, name, value);
}

static void
assert_msg_matches_and_has_tag(PatternDB *patterndb, const gchar *pattern, const gchar *tag, gboolean set)
{
  LogMessage *msg;
  gboolean result;

  msg = _construct_message("prog1", pattern);
  result = _process(patterndb, msg);
  cr_assert(result, "patterndb expected to match but it didn't");

  if (set)
    assert_log_message_has_tag(msg, tag);
  else
    assert_log_message_doesnt_have_tag(msg, tag);
  log_msg_unref(msg);
}

void
assert_output_message_nvpair_equals(gint ndx, const gchar *name, const gchar *value)
{
  assert_log_message_value(_get_output_message(ndx), log_msg_get_value_handle(name), value);
}

void
assert_msg_matches_and_output_message_nvpair_equals_with_timeout(PatternDB *patterndb, const gchar *pattern,
    gint timeout, gint ndx,
    const gchar *name, const gchar *value)
{
  LogMessage *msg;

  msg = _construct_message("prog2", pattern);
  _process(patterndb, msg);
  _advance_time(patterndb, timeout);

  assert_output_message_nvpair_equals(ndx, name, value);
  log_msg_unref(msg);
}

static void
assert_no_such_output_message(gint ndx)
{
  cr_assert(ndx >= messages->len, "Unexpected message generated at %d index\n", ndx);
}

void
assert_msg_matches_and_no_such_output_message(PatternDB *patterndb, const gchar *pattern, gint ndx)
{
  LogMessage *msg;

  msg = _construct_message("prog2", pattern);
  _process(patterndb, msg);

  assert_no_such_output_message(ndx);
  log_msg_unref(msg);
}

void
assert_msg_matches_and_output_message_nvpair_equals(PatternDB *patterndb, const gchar *pattern, gint ndx,
                                                    const gchar *name,
                                                    const gchar *value)
{
  assert_msg_matches_and_output_message_nvpair_equals_with_timeout(patterndb, pattern, 0, ndx, name, value);
}

void
assert_output_message_has_tag(gint ndx, const gchar *tag, gboolean set)
{
  if (set)
    assert_log_message_has_tag(_get_output_message(ndx), tag);
  else
    assert_log_message_doesnt_have_tag(_get_output_message(ndx), tag);
}

void
assert_msg_matches_and_output_message_has_tag_with_timeout(PatternDB *patterndb, const gchar *pattern, gint timeout,
                                                           gint ndx,
                                                           const gchar *tag, gboolean set)
{
  LogMessage *msg;

  msg = _construct_message("prog2", pattern);
  _process(patterndb, msg);
  _advance_time(patterndb, timeout);
  assert_output_message_has_tag(ndx, tag, set);
  log_msg_unref(msg);
}

void
assert_msg_matches_and_output_message_has_tag(PatternDB *patterndb, const gchar *pattern, gint ndx, const gchar *tag,
                                              gboolean set)
{
  assert_msg_matches_and_output_message_has_tag_with_timeout(patterndb, pattern, 0, ndx, tag, set);
}

Test(pattern_db, test_simple_rule_without_context_or_actions)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_ruletest_skeleton, &filename);

  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag(patterndb, "simple-message", ".classifier.system", TRUE);

  /* tag assignment based on <tags/> */
  assert_msg_matches_and_nvpair_equals(patterndb, "simple-message", "TAGS",
                                       ".classifier.system,simple-msg-tag1,simple-msg-tag2");

  assert_msg_matches_and_nvpair_equals(patterndb, "simple-message", "simple-msg-value-1", "value1");
  assert_msg_matches_and_nvpair_equals(patterndb, "simple-message", "simple-msg-value-2", "value2");
  assert_msg_matches_and_nvpair_equals(patterndb, "simple-message", "simple-msg-host", MYHOST);

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_correllation_rule_without_actions)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_ruletest_skeleton, &filename);

  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag(patterndb, "correllated-message-based-on-pid", ".classifier.system", TRUE);
  assert_msg_matches_and_nvpair_equals(patterndb, "correllated-message-based-on-pid", "correllated-msg-context-id",
                                       MYPID);
  assert_msg_matches_and_nvpair_equals(patterndb, "correllated-message-based-on-pid", "correllated-msg-context-length",
                                       "1");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals(patterndb, "correllated-message-based-on-pid", "correllated-msg-context-length",
                                       "2");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals(patterndb, "correllated-message-based-on-pid", "correllated-msg-context-length",
                                       "3");

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_correllation_rule_with_action_on_match)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_ruletest_skeleton, &filename);

  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag(patterndb, "correllated-message-with-action-on-match", ".classifier.violation", TRUE);

  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "correllated-message-with-action-on-match", 1, "MESSAGE",
                                                      "generated-message-on-match");
  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "correllated-message-with-action-on-match", 1,
                                                      "context-id", "999");
  assert_msg_matches_and_output_message_has_tag(patterndb, "correllated-message-with-action-on-match", 1,
                                                "correllated-msg-tag",
                                                TRUE);

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_correllation_rule_with_action_on_timeout)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_ruletest_skeleton, &filename);

  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag(patterndb, "correllated-message-with-action-on-timeout", ".classifier.violation", TRUE);

  assert_msg_matches_and_output_message_nvpair_equals_with_timeout(patterndb,
      "correllated-message-with-action-on-timeout", 60, 1,
      "MESSAGE", "generated-message-on-timeout");

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_correllation_rule_with_action_condition)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_ruletest_skeleton, &filename);

  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag(patterndb, "correllated-message-with-action-condition", ".classifier.violation", TRUE);

  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "correllated-message-with-action-condition", 1,
                                                      "MESSAGE",
                                                      "generated-message-on-condition");

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_correllation_rule_with_rate_limited_action)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_ruletest_skeleton, &filename);

  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag(patterndb, "correllated-message-with-rate-limited-action", ".classifier.violation",
                                 TRUE);

  /* messages in the output:
   * [0] trigger
   * [1] GENERATED (as rate limit was met)
   * [2] trigger
   * [3] trigger
   * [4] trigger
   * [5] GENERATED (as rate limit was met again due to advance time */

  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "correllated-message-with-rate-limited-action", 1,
                                                      "MESSAGE",
                                                      "generated-message-rate-limit");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_no_such_output_message(patterndb, "correllated-message-with-rate-limited-action", 3);
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_no_such_output_message(patterndb, "correllated-message-with-rate-limited-action", 4);
  _dont_reset_patterndb_state_for_the_next_call();
  _advance_time(patterndb, 120);
  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "correllated-message-with-rate-limited-action", 5,
                                                      "MESSAGE",
                                                      "generated-message-rate-limit");

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_simple_rule_with_action_on_match)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_ruletest_skeleton, &filename);

  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag(patterndb, "simple-message-with-action-on-match", ".classifier.violation", TRUE);

  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "simple-message-with-action-on-match", 1, "MESSAGE",
                                                      "generated-message-on-match");
  assert_msg_matches_and_output_message_has_tag(patterndb, "simple-message-with-action-on-match", 1, "simple-msg-tag",
                                                TRUE);

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_simple_rule_with_rate_limited_action)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_ruletest_skeleton, &filename);

  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag(patterndb, "simple-message-with-rate-limited-action", ".classifier.violation", TRUE);

  /* messages in the output:
   * [0] trigger
   * [1] GENERATED (as rate limit was met)
   * [2] trigger
   * [3] trigger
   * [4] trigger
   * [5] GENERATED (as rate limit was met again due to advance time */

  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "simple-message-with-rate-limited-action", 1, "MESSAGE",
                                                      "generated-message-rate-limit");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_no_such_output_message(patterndb, "simple-message-with-rate-limited-action", 3);
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_no_such_output_message(patterndb, "simple-message-with-rate-limited-action", 4);
  _dont_reset_patterndb_state_for_the_next_call();
  _advance_time(patterndb, 120);
  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "simple-message-with-rate-limited-action", 5, "MESSAGE",
                                                      "generated-message-rate-limit");

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_simple_rule_with_action_condition)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_ruletest_skeleton, &filename);

  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag(patterndb, "simple-message-with-action-condition", ".classifier.violation", TRUE);

  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "simple-message-with-action-condition", 1, "MESSAGE",
                                                      "generated-message-on-condition");

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_correllation_rule_with_create_context)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_ruletest_skeleton, &filename);

  assert_msg_matches_and_nvpair_equals(patterndb, "simple-message-with-action-to-create-context", ".classifier.rule_id",
                                       "12");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals(patterndb, "correllated-message-that-uses-context-created-by-rule-id#12",
                                       "triggering-message", "context message assd");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals(patterndb, "correllated-message-that-uses-context-created-by-rule-id#12",
                                       "PROGRAM", "prog1");


  assert_msg_matches_and_nvpair_equals(patterndb, "correllated-message-with-action-to-create-context",
                                       ".classifier.rule_id", "14");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals(patterndb, "correllated-message-that-uses-context-created-by-rule-id#14",
                                       "triggering-message", "context message 1001 assd");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals(patterndb, "correllated-message-that-uses-context-created-by-rule-id#14",
                                       "PROGRAM", "prog1");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals(patterndb, "correllated-message-that-uses-context-created-by-rule-id#14",
                                       "triggering-message-context-id", "1001");

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_patterndb_loads_a_syntactically_complete_xml_properly)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_complete_syntax, &filename);
  /* check we did indeed load the patterns */
  assert_msg_matches_and_has_tag(patterndb, "simple-message", ".classifier.system", TRUE);
  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, pdbtest_patterndb_message_property_inheritance_enabled)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_inheritance_enabled_skeleton, &filename);
  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "pattern-with-inheritance-enabled", 1, "MESSAGE",
                                                      "pattern-with-inheritance-enabled");
  assert_msg_matches_and_output_message_has_tag(patterndb, "pattern-with-inheritance-enabled", 1, "basetag1", TRUE);
  assert_msg_matches_and_output_message_has_tag(patterndb, "pattern-with-inheritance-enabled", 1, "basetag2", TRUE);
  assert_msg_matches_and_output_message_has_tag(patterndb, "pattern-with-inheritance-enabled", 1, "actiontag", TRUE);
  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "pattern-with-inheritance-enabled", 1, "actionkey",
                                                      "actionvalue");

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_patterndb_message_property_inheritance_disabled)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_inheritance_disabled_skeleton, &filename);

  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "pattern-with-inheritance-disabled", 1, "MESSAGE", NULL);
  assert_msg_matches_and_output_message_has_tag(patterndb, "pattern-with-inheritance-disabled", 1, "basetag1", FALSE);
  assert_msg_matches_and_output_message_has_tag(patterndb, "pattern-with-inheritance-disabled", 1, "basetag2", FALSE);
  assert_msg_matches_and_output_message_has_tag(patterndb, "pattern-with-inheritance-disabled", 1, "actiontag", TRUE);
  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "pattern-with-inheritance-disabled", 1, "actionkey",
                                                      "actionvalue");

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_patterndb_message_property_inheritance_context)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_inheritance_context_skeleton, &filename);

  _feed_message_to_correllation_state(patterndb, "prog2", "pattern-with-inheritance-context", "merged1", "merged1");
  _feed_message_to_correllation_state(patterndb, "prog2", "pattern-with-inheritance-context", "merged2", "merged2");
  _advance_time(patterndb, 60);

  assert_output_message_nvpair_equals(2, "MESSAGE", "action message");
  assert_output_message_nvpair_equals(2, "merged1", "merged1");
  assert_output_message_nvpair_equals(2, "merged2", "merged2");
  assert_output_message_has_tag(2, "actiontag", TRUE);

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_patterndb_context_length)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(pdb_msg_count_skeleton, &filename);

  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "pattern13", 1, "CONTEXT_LENGTH", "2");
  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "pattern14", 1, "CONTEXT_LENGTH", "2");

  assert_msg_with_program_matches_and_nvpair_equals(patterndb, "prog2", "pattern15-a", "p15", "-a");

  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_output_message_nvpair_equals(patterndb, "pattern15-b", 2, "fired", "true");

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

typedef struct _patterndb_test_param
{
  const gchar *pattern_db;
  const gchar *program;
  const gchar *message;
  const gchar *name;
  const gchar *expected_value;
} PatternDBTestParam;

ParameterizedTestParameters(pattern_db, test_rules)
{
  static PatternDBTestParam parser_params[] =
  {
    // test_conflicting_rules_with_different_parsers
    {
      .pattern_db = pdb_conflicting_rules_with_different_parsers,
      .program = "prog1",
      .message = "pattern foobar ",
      .name = ".classifier.rule_id",
      .expected_value = "11"
    },
    {
      .pattern_db = pdb_conflicting_rules_with_different_parsers,
      .program = "prog1",
      .message = "pattern foobar tail",
      .name = ".classifier.rule_id",
      .expected_value = "12"
    },
    {
      .pattern_db = pdb_conflicting_rules_with_different_parsers,
      .program = "prog1",
      .message = "pattern foobar something else",
      .name = ".classifier.rule_id",
      .expected_value = "11"
    },
    // test_conflicting_rules_with_the_same_parsers
    {
      .pattern_db = pdb_conflicting_rules_with_the_same_parsers,
      .program = "prog1",
      .message = "pattern foobar ",
      .name = ".classifier.rule_id",
      .expected_value = "11"
    },
    {
      .pattern_db = pdb_conflicting_rules_with_the_same_parsers,
      .program = "prog1",
      .message = "pattern foobar tail",
      .name = ".classifier.rule_id",
      .expected_value = "12"
    },
    {
      .pattern_db = pdb_conflicting_rules_with_the_same_parsers,
      .program = "prog1",
      .message = "pattern foobar something else",
      .name = ".classifier.rule_id",
      .expected_value = "11"
    },
  };

  return cr_make_param_array(PatternDBTestParam, parser_params, G_N_ELEMENTS(parser_params));
}

ParameterizedTest(PatternDBTestParam *param, pattern_db, test_rules)
{
  gchar *filename;
  PatternDB *patterndb = _create_pattern_db(param->pattern_db, &filename);
  assert_msg_with_program_matches_and_nvpair_equals(patterndb, param->program, param->message, param->name,
                                                    param->expected_value);
  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

Test(pattern_db, test_tag_outside_of_rule_skeleton)
{
  PatternDB *patterndb = pattern_db_new();

  char *filename;
  g_file_open_tmp("patterndbXXXXXX.xml", &filename, NULL);
  g_file_set_contents(filename, pdb_tag_outside_of_rule_skeleton, strlen(pdb_tag_outside_of_rule_skeleton), NULL);

  cr_assert_not(pattern_db_reload_ruleset(patterndb, configuration, filename),
                "successfully loaded an invalid patterndb file");

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

const gchar *dirs[] =
{
  "pathutils_get_filenames",
  "pathutils_get_filenames/testdir",
  "pathutils_get_filenames/testdir2"
};
const gchar *files[] =
{
  "pathutils_get_filenames/test.file",
  "pathutils_get_filenames/test2.file",
  "pathutils_get_filenames/testdir/test.file",
  "pathutils_get_filenames/testdir2/test23.file",
  "pathutils_get_filenames/testdir2/test22.file"
};
size_t dirs_len = G_N_ELEMENTS(dirs);
size_t files_len = G_N_ELEMENTS(files);

void
test_pdb_get_filenames_setup(void)
{
  for (gint i = 0; i < dirs_len; ++i)
    g_assert(mkdir(dirs[i], S_IRUSR | S_IWUSR | S_IXUSR) >= 0);

  for (gint i = 0; i < files_len; ++i)
    {
      int fd = open(files[i], O_CREAT | O_RDWR, 0644);
      g_assert(fd >= 0);
      g_assert(close(fd) == 0);
    }
}

void
test_pdb_get_filenames_teardown(void)
{
  for (gint i = 0; i < files_len; ++i)
    g_assert(g_remove(files[i]) == 0);
  for (gint i = dirs_len - 1; i >= 0; --i)
    g_assert(g_remove(dirs[i]) == 0);
}

Test(test_pathutils, test_pdb_get_filenames, .init = test_pdb_get_filenames_setup,
     .fini = test_pdb_get_filenames_teardown)
{
  GError *error;
  const gchar *expected[] =
  {
    "pathutils_get_filenames/test2.file",
    "pathutils_get_filenames/testdir2/test22.file",
    "pathutils_get_filenames/testdir2/test23.file"
  };
  guint expected_len = G_N_ELEMENTS(expected);
  GPtrArray *filenames = pdb_get_filenames("pathutils_get_filenames", TRUE, "*test2*", &error);

  cr_assert(filenames);
  cr_assert(filenames->len == expected_len);

  pdb_sort_filenames(filenames);

  for (guint i = 0; i < filenames->len; ++i)
    cr_assert_str_eq(g_ptr_array_index(filenames, i), expected[i]);

  g_ptr_array_free(filenames, TRUE);
}

void setup(void)
{
  app_startup();
  msg_init(TRUE);
  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "basicfuncs");
  cfg_load_module(configuration, "syslogformat");
  pattern_db_global_init();
}

void teardown(void)
{
  app_shutdown();
}

TestSuite(pattern_db, .init = setup, .fini = teardown);
