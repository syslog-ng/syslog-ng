/*
 * Copyright (c) 2010-2015 Balabit
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

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>

#define MYHOST "MYHOST"
#define MYPID "999"

PatternDB *patterndb;
gchar *filename;
GPtrArray *messages;
gboolean keep_patterndb_state = FALSE;

static void
_emit_func(LogMessage *msg, gboolean synthetic, gpointer user_data)
{
  g_ptr_array_add(messages, log_msg_ref(msg));
}

static void
assert_pdb_file_valid(const gchar *filename_, const gchar *pdb)
{
  GError *error = NULL;
  gboolean success;

  success = pdb_file_validate_in_tests(filename_, &error);
  assert_true(success, "Error validating patterndb, error=%s\n>>>\n%s\n<<<", error ? error->message : "unknown", pdb);
  g_clear_error(&error);
}

static void
_load_pattern_db_from_string(const gchar *pdb)
{
  patterndb = pattern_db_new();
  messages = g_ptr_array_new();

  pattern_db_set_emit_func(patterndb, _emit_func, NULL);

  g_file_open_tmp("patterndbXXXXXX.xml", &filename, NULL);
  g_file_set_contents(filename, pdb, strlen(pdb), NULL);

  assert_pdb_file_valid(filename, pdb);

  assert_true(pattern_db_reload_ruleset(patterndb, configuration, filename), "Error loading ruleset [[[%s]]]", pdb);
  assert_string(pattern_db_get_ruleset_pub_date(patterndb), "2010-02-22", "Invalid pubdate");
}

static void
_destroy_pattern_db(void)
{
  if (messages)
    {
      g_ptr_array_foreach(messages, (GFunc) log_msg_unref, NULL);
      g_ptr_array_free(messages, TRUE);
    }
  messages = NULL;
  pattern_db_free(patterndb);
  patterndb = NULL;

  g_unlink(filename);
  g_free(filename);
  filename = NULL;
}

static void
_reset_pattern_db_state(void)
{
  pattern_db_forget_state(patterndb);
  g_ptr_array_foreach(messages, (GFunc) log_msg_unref, NULL);
  g_ptr_array_set_size(messages, 0);
}


static gboolean
_process(LogMessage *msg)
{
  if (!keep_patterndb_state)
    _reset_pattern_db_state();
  keep_patterndb_state = FALSE;
  return pattern_db_process(patterndb, msg);
}

static void
_dont_reset_patterndb_state_for_the_next_call(void)
{
  keep_patterndb_state = TRUE;
}

static void
_advance_time(gint timeout)
{
  if (timeout)
    pattern_db_advance_time(patterndb, timeout + 1);
}

static LogMessage *
_get_output_message(gint ndx)
{
  assert_true(ndx < messages->len, "Expected the %d. message, but no such message was returned by patterndb\n", ndx);
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
  msg->timestamps[LM_TS_STAMP].tv_sec = msg->timestamps[LM_TS_RECVD].tv_sec;

  return msg;
}

static LogMessage *
_construct_message(const gchar *program, const gchar *message)
{
  return _construct_message_with_nvpair(program, message, NULL, NULL);
}

static void
_feed_message_to_correllation_state(const gchar *program, const gchar *message, const gchar *name, const gchar *value)
{
  LogMessage *msg;
  gboolean result;

  msg = _construct_message_with_nvpair(program, message, name, value);
  result = _process(msg);
  log_msg_unref(msg);
  assert_true(result, "patterndb expected to match but it didn't");
  _dont_reset_patterndb_state_for_the_next_call();
}

static void
assert_msg_with_program_matches_and_nvpair_equals(const gchar *program, const gchar *message,
                                                  const gchar *name, const gchar *expected_value)
{
  LogMessage *msg;
  gboolean result;

  msg = _construct_message(program, message);
  result = _process(msg);
  assert_true(result, "patterndb expected to match but it didn't");
  assert_log_message_value(msg, log_msg_get_value_handle(name), expected_value);
  log_msg_unref(msg);
}

static void
assert_msg_doesnot_match(const gchar *message)
{
  LogMessage *msg;
  gboolean result;

  msg = _construct_message("prog1", message);
  result = _process(msg);
  assert_false(result, "patterndb expected to match but it didn't");
  assert_log_message_value(msg, log_msg_get_value_handle(".classifier.class"), "unknown");
  assert_log_message_has_tag(msg, ".classifier.unknown");
  log_msg_unref(msg);
}

static void
assert_msg_matches_and_nvpair_equals(const gchar *pattern, const gchar *name, const gchar *value)
{
  assert_msg_with_program_matches_and_nvpair_equals("prog1", pattern, name, value);
}

static void
assert_msg_matches_and_has_tag(const gchar *pattern, const gchar *tag, gboolean set)
{
  LogMessage *msg;
  gboolean result;

  msg = _construct_message("prog1", pattern);
  result = _process(msg);
  assert_true(result, "patterndb expected to match but it didn't");

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
assert_msg_matches_and_output_message_nvpair_equals_with_timeout(const gchar *pattern, gint timeout, gint ndx,
    const gchar *name, const gchar *value)
{
  LogMessage *msg;

  msg = _construct_message("prog2", pattern);
  _process(msg);
  _advance_time(timeout);

  assert_output_message_nvpair_equals(ndx, name, value);
  log_msg_unref(msg);
}

static void
assert_no_such_output_message(gint ndx)
{
  assert_true(ndx >= messages->len, "Unexpected message generated at %d index\n", ndx);
}

void
assert_msg_matches_and_no_such_output_message(const gchar *pattern, gint ndx)
{
  LogMessage *msg;

  msg = _construct_message("prog2", pattern);
  _process(msg);

  assert_no_such_output_message(ndx);
  log_msg_unref(msg);
}

void
assert_msg_matches_and_output_message_nvpair_equals(const gchar *pattern, gint ndx, const gchar *name,
                                                    const gchar *value)
{
  assert_msg_matches_and_output_message_nvpair_equals_with_timeout(pattern, 0, ndx, name, value);
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
assert_msg_matches_and_output_message_has_tag_with_timeout(const gchar *pattern, gint timeout, gint ndx,
                                                           const gchar *tag, gboolean set)
{
  LogMessage *msg;

  msg = _construct_message("prog2", pattern);
  _process(msg);
  _advance_time(timeout);
  assert_output_message_has_tag(ndx, tag, set);
  log_msg_unref(msg);
}

void
assert_msg_matches_and_output_message_has_tag(const gchar *pattern, gint ndx, const gchar *tag, gboolean set)
{
  assert_msg_matches_and_output_message_has_tag_with_timeout(pattern, 0, ndx, tag, set);
}

gchar *pdb_conflicting_rules_with_different_parsers = "<patterndb version='4' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog1</pattern>\
   <pattern>prog2</pattern>\
  </patterns>\
  <rules>\
    <!-- different parsers at the same location -->\
    <rule provider='test' id='11' class='short'>\
     <patterns>\
      <pattern>pattern @ESTRING:foo1: @</pattern>\
     </patterns>\
    </rule>\
    <rule provider='test' id='12' class='long'>\
     <patterns>\
      <pattern>pattern @ESTRING:foo2: @tail</pattern>\
     </patterns>\
    </rule>\
  </rules>\
 </ruleset>\
</patterndb>";

static void
test_conflicting_rules_with_different_parsers(void)
{
  _load_pattern_db_from_string(pdb_conflicting_rules_with_different_parsers);

  /* the short rule matches completely, the other doesn't */
  assert_msg_matches_and_nvpair_equals("pattern foobar ", ".classifier.rule_id", "11");

  /* we have a longer rule, even though both rules would match */
  assert_msg_matches_and_nvpair_equals("pattern foobar tail", ".classifier.rule_id", "12");

  /* the longest rule didn't match, so use the shorter one as a partial match */
  assert_msg_matches_and_nvpair_equals("pattern foobar something else", ".classifier.rule_id", "11");
  _destroy_pattern_db();
}

gchar *pdb_conflicting_rules_with_the_same_parsers = "<patterndb version='4' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog1</pattern>\
   <pattern>prog2</pattern>\
  </patterns>\
  <rules>\
    <!-- different parsers at the same location -->\
    <rule provider='test' id='11' class='short'>\
     <patterns>\
      <pattern>pattern @ESTRING:foo: @</pattern>\
     </patterns>\
    </rule>\
    <rule provider='test' id='12' class='long'>\
     <patterns>\
      <pattern>pattern @ESTRING:foo: @tail</pattern>\
     </patterns>\
    </rule>\
  </rules>\
 </ruleset>\
</patterndb>";

static void
test_conflicting_rules_with_the_same_parsers(void)
{
  _load_pattern_db_from_string(pdb_conflicting_rules_with_the_same_parsers);

  /* the short rule matches completely, the other doesn't */
  assert_msg_matches_and_nvpair_equals("pattern foobar ", ".classifier.rule_id", "11");

  /* we have a longer rule, even though both rules would match */
  assert_msg_matches_and_nvpair_equals("pattern foobar tail", ".classifier.rule_id", "12");

  /* the longest rule didn't match, so use the shorter one as a partial match */
  assert_msg_matches_and_nvpair_equals("pattern foobar something else", ".classifier.rule_id", "11");
  _destroy_pattern_db();
}


/* pdb skeleton used to test patterndb rule actions. E.g. whenever a rule
 * matches, certain actions described in the rule need to be performed.
 * This tests those */
gchar *pdb_ruletest_skeleton = "<patterndb version='5' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <description>This is a test set</description>\
  <patterns>\
    <pattern>prog1</pattern>\
    <pattern>prog2</pattern>\
  </patterns>\
  <rules>\
    <rule provider='test' id='10' class='system' context-scope='program'>\
     <patterns>\
      <pattern>simple-message</pattern>\
     </patterns>\
     <tags>\
      <tag>simple-msg-tag1</tag>\
      <tag>simple-msg-tag2</tag>\
     </tags>\
     <values>\
      <value name='simple-msg-value-1'>value1</value>\
      <value name='simple-msg-value-2'>value2</value>\
      <value name='simple-msg-host'>${HOST}</value>\
     </values>\
    </rule>\
    <rule provider='test' id='10a' class='system' context-scope='program' context-id='$PID' context-timeout='60'>\
     <patterns>\
      <pattern>correllated-message-based-on-pid</pattern>\
     </patterns>\
     <values>\
      <value name='correllated-msg-context-id'>${CONTEXT_ID}</value>\
      <value name='correllated-msg-context-length'>$(context-length)</value>\
     </values>\
    </rule>\
    <rule provider='test' id='10b' class='violation' context-scope='program' context-id='$PID' context-timeout='60'>\
     <patterns>\
      <pattern>correllated-message-with-action-on-match</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match'>\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-on-match</value>\
             <value name='context-id'>${CONTEXT_ID}</value>\
           </values>\
           <tags>\
             <tag>correllated-msg-tag</tag>\
           </tags>\
         </message>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='10c' class='violation' context-scope='program' context-id='$PID' context-timeout='60'>\
     <patterns>\
      <pattern>correllated-message-with-action-on-timeout</pattern>\
     </patterns>\
     <actions>\
       <action trigger='timeout'>\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-on-timeout</value>\
           </values>\
         </message>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='10d' class='violation' context-scope='program' context-id='$PID' context-timeout='60'>\
     <patterns>\
      <pattern>correllated-message-with-action-condition</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match' condition='\"${PID}\" ne \"" MYPID "\"' >\
         <message>\
           <values>\
             <value name='MESSAGE'>not-generated-message</value>\
           </values>\
         </message>\
       </action>\
       <action trigger='match' condition='\"${PID}\" eq \"" MYPID "\"' >\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-on-condition</value>\
           </values>\
         </message>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='10e' class='violation' context-scope='program' context-id='$PID' context-timeout='60'>\
     <patterns>\
      <pattern>correllated-message-with-rate-limited-action</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match' rate='1/60'>\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-rate-limit</value>\
           </values>\
         </message>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='11b' class='violation'>\
     <patterns>\
      <pattern>simple-message-with-action-on-match</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match'>\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-on-match</value>\
             <value name='context-id'>${CONTEXT_ID}</value>\
           </values>\
           <tags>\
             <tag>simple-msg-tag</tag>\
           </tags>\
         </message>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='11d' class='violation'>\
     <patterns>\
      <pattern>simple-message-with-action-condition</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match' condition='\"${PID}\" ne \"" MYPID "\"' >\
         <message>\
           <values>\
             <value name='MESSAGE'>not-generated-message</value>\
           </values>\
         </message>\
       </action>\
       <action trigger='match' condition='\"${PID}\" eq \"" MYPID "\"' >\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-on-condition</value>\
           </values>\
         </message>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='11e' class='violation'>\
     <patterns>\
      <pattern>simple-message-with-rate-limited-action</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match' rate='1/60'>\
         <message>\
           <values>\
             <value name='MESSAGE'>generated-message-rate-limit</value>\
           </values>\
         </message>\
       </action>\
     </actions>\
     <examples>\
       <example>\
         <test_message program='prog1'>simple-message-with-rate-limited-action</test_message>\
         <test_values>\
            <test_value name='PROGRAM'>prog1</test_value>\
            <test_value name='MESSAGE'>foobar</test_value>\
         </test_values>\
       </example>\
       <example>\
         <test_message program='prog2'>simple-message-with-rate-limited-action</test_message>\
       </example>\
     </examples>\
    </rule>\
    <rule provider='test' id='12' class='violation'>\
     <patterns>\
      <pattern>simple-message-with-action-to-create-context</pattern>\
     </patterns>\
     <actions>\
       <action trigger='match'>\
         <create-context context-id='1000' context-timeout='60' context-scope='program'>\
           <message inherit-properties='context'>\
             <values>\
               <value name='MESSAGE'>context message</value>\
             </values>\
           </message>\
         </create-context>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='13' class='violation' context-id='1000' context-timeout='60' context-scope='program'>\
     <patterns>\
      <pattern>correllated-message-that-uses-context-created-by-rule-id#12</pattern>\
     </patterns>\
     <values>\
       <value name='triggering-message'>${MESSAGE}@1 assd</value>\
     </values>\
    </rule>\
    <rule provider='test' id='14' class='violation' context-id='1001' context-timeout='60' context-scope='program'>\
     <patterns>\
      <pattern>correllated-message-with-action-to-create-context</pattern>\
     </patterns>\
     <values>\
       <value name='rule-msg-context-id'>${.classifier.context_id}</value>\
     </values>\
     <actions>\
       <action trigger='match'>\
         <create-context context-id='1002' context-timeout='60' context-scope='program'>\
           <message inherit-properties='context'>\
             <values>\
               <!-- we should inherit from the LogMessage matching this rule and not the to be created context -->\
               <value name='MESSAGE'>context message ${rule-msg-context-id}</value>\
             </values>\
           </message>\
         </create-context>\
       </action>\
     </actions>\
    </rule>\
    <rule provider='test' id='15' class='violation' context-id='1002' context-timeout='60' context-scope='program'>\
     <patterns>\
      <pattern>correllated-message-that-uses-context-created-by-rule-id#14</pattern>\
     </patterns>\
     <values>\
       <value name='triggering-message'>${MESSAGE}@1 assd</value>\
       <value name='triggering-message-context-id'>$(grep ('${rule-msg-context-id}' ne '') ${rule-msg-context-id})</value>\
     </values>\
    </rule>\
  </rules>\
 </ruleset>\
</patterndb>";

static void
test_simple_rule_without_context_or_actions(void)
{
  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag("simple-message", ".classifier.system", TRUE);

  /* tag assignment based on <tags/> */
  assert_msg_matches_and_nvpair_equals("simple-message", "TAGS", ".classifier.system,simple-msg-tag1,simple-msg-tag2");

  assert_msg_matches_and_nvpair_equals("simple-message", "simple-msg-value-1", "value1");
  assert_msg_matches_and_nvpair_equals("simple-message", "simple-msg-value-2", "value2");
  assert_msg_matches_and_nvpair_equals("simple-message", "simple-msg-host", MYHOST);
}

static void
test_correllation_rule_without_actions(void)
{
  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag("correllated-message-based-on-pid", ".classifier.system", TRUE);
  assert_msg_matches_and_nvpair_equals("correllated-message-based-on-pid", "correllated-msg-context-id", MYPID);
  assert_msg_matches_and_nvpair_equals("correllated-message-based-on-pid", "correllated-msg-context-length", "1");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals("correllated-message-based-on-pid", "correllated-msg-context-length", "2");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals("correllated-message-based-on-pid", "correllated-msg-context-length", "3");
}

static void
test_correllation_rule_with_action_on_match(void)
{
  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag("correllated-message-with-action-on-match", ".classifier.violation", TRUE);

  assert_msg_matches_and_output_message_nvpair_equals("correllated-message-with-action-on-match", 1, "MESSAGE",
                                                      "generated-message-on-match");
  assert_msg_matches_and_output_message_nvpair_equals("correllated-message-with-action-on-match", 1, "context-id", "999");
  assert_msg_matches_and_output_message_has_tag("correllated-message-with-action-on-match", 1, "correllated-msg-tag",
                                                TRUE);
}

static void
test_correllation_rule_with_action_on_timeout(void)
{
  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag("correllated-message-with-action-on-timeout", ".classifier.violation", TRUE);

  assert_msg_matches_and_output_message_nvpair_equals_with_timeout("correllated-message-with-action-on-timeout", 60, 1,
      "MESSAGE", "generated-message-on-timeout");
}

static void
test_correllation_rule_with_action_condition(void)
{
  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag("correllated-message-with-action-condition", ".classifier.violation", TRUE);

  assert_msg_matches_and_output_message_nvpair_equals("correllated-message-with-action-condition", 1, "MESSAGE",
                                                      "generated-message-on-condition");
}

static void
test_correllation_rule_with_rate_limited_action(void)
{
  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag("correllated-message-with-rate-limited-action", ".classifier.violation", TRUE);

  /* messages in the output:
   * [0] trigger
   * [1] GENERATED (as rate limit was met)
   * [2] trigger
   * [3] trigger
   * [4] trigger
   * [5] GENERATED (as rate limit was met again due to advance time */

  assert_msg_matches_and_output_message_nvpair_equals("correllated-message-with-rate-limited-action", 1, "MESSAGE",
                                                      "generated-message-rate-limit");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_no_such_output_message("correllated-message-with-rate-limited-action", 3);
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_no_such_output_message("correllated-message-with-rate-limited-action", 4);
  _dont_reset_patterndb_state_for_the_next_call();
  _advance_time(120);
  assert_msg_matches_and_output_message_nvpair_equals("correllated-message-with-rate-limited-action", 5, "MESSAGE",
                                                      "generated-message-rate-limit");
}

static void
test_simple_rule_with_action_on_match(void)
{
  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag("simple-message-with-action-on-match", ".classifier.violation", TRUE);

  assert_msg_matches_and_output_message_nvpair_equals("simple-message-with-action-on-match", 1, "MESSAGE",
                                                      "generated-message-on-match");
  assert_msg_matches_and_output_message_has_tag("simple-message-with-action-on-match", 1, "simple-msg-tag", TRUE);
}

static void
test_simple_rule_with_rate_limited_action(void)
{
  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag("simple-message-with-rate-limited-action", ".classifier.violation", TRUE);

  /* messages in the output:
   * [0] trigger
   * [1] GENERATED (as rate limit was met)
   * [2] trigger
   * [3] trigger
   * [4] trigger
   * [5] GENERATED (as rate limit was met again due to advance time */

  assert_msg_matches_and_output_message_nvpair_equals("simple-message-with-rate-limited-action", 1, "MESSAGE",
                                                      "generated-message-rate-limit");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_no_such_output_message("simple-message-with-rate-limited-action", 3);
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_no_such_output_message("simple-message-with-rate-limited-action", 4);
  _dont_reset_patterndb_state_for_the_next_call();
  _advance_time(120);
  assert_msg_matches_and_output_message_nvpair_equals("simple-message-with-rate-limited-action", 5, "MESSAGE",
                                                      "generated-message-rate-limit");
}


static void
test_simple_rule_with_action_condition(void)
{
  /* tag assigned based on "class" */
  assert_msg_matches_and_has_tag("simple-message-with-action-condition", ".classifier.violation", TRUE);

  assert_msg_matches_and_output_message_nvpair_equals("simple-message-with-action-condition", 1, "MESSAGE",
                                                      "generated-message-on-condition");
}

static void
test_correllation_rule_with_create_context(void)
{
  assert_msg_matches_and_nvpair_equals("simple-message-with-action-to-create-context", ".classifier.rule_id", "12");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals("correllated-message-that-uses-context-created-by-rule-id#12",
                                       "triggering-message", "context message assd");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals("correllated-message-that-uses-context-created-by-rule-id#12", "PROGRAM", "prog1");


  assert_msg_matches_and_nvpair_equals("correllated-message-with-action-to-create-context", ".classifier.rule_id", "14");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals("correllated-message-that-uses-context-created-by-rule-id#14",
                                       "triggering-message", "context message 1001 assd");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals("correllated-message-that-uses-context-created-by-rule-id#14", "PROGRAM", "prog1");
  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_nvpair_equals("correllated-message-that-uses-context-created-by-rule-id#14",
                                       "triggering-message-context-id", "1001");
}

static void
test_patterndb_rule(void)
{
  _load_pattern_db_from_string(pdb_ruletest_skeleton);

  test_simple_rule_without_context_or_actions();
  test_correllation_rule_without_actions();
  test_correllation_rule_with_action_on_match();
  test_correllation_rule_with_action_on_timeout();
  test_correllation_rule_with_action_condition();
  test_correllation_rule_with_rate_limited_action();

  test_simple_rule_with_action_on_match();
  test_simple_rule_with_action_condition();
  test_simple_rule_with_rate_limited_action();

  test_correllation_rule_with_create_context();

  assert_msg_doesnot_match("non-matching-pattern");
  _destroy_pattern_db();
}

const gchar *pdb_complete_syntax = "\
<patterndb version='5' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <url>http://foobar.org/</url>\
  <urls>\
    <url>http://foobar.org/1</url>\
    <url>http://foobar.org/2</url>\
  </urls>\
  <description>This is a test set</description>\
  <patterns>\
    <pattern>prog2</pattern>\
    <pattern>prog3</pattern>\
  </patterns>\
  <pattern>prog1</pattern>\
  <rules>\
    <rule provider='test' id='10' class='system' context-id='foobar' context-scope='program'>\
     <description>This is a rule description</description>\
     <urls>\
       <url>http://foobar.org/1</url>\
       <url>http://foobar.org/2</url>\
     </urls>\
     <patterns>\
      <pattern>simple-message</pattern>\
      <pattern>simple-message-alternative</pattern>\
     </patterns>\
     <tags>\
      <tag>simple-msg-tag1</tag>\
      <tag>simple-msg-tag2</tag>\
     </tags>\
     <values>\
      <value name='simple-msg-value-1'>value1</value>\
      <value name='simple-msg-value-2'>value2</value>\
      <value name='simple-msg-host'>${HOST}</value>\
     </values>\
     <examples>\
       <example>\
         <test_message program='foobar'>This is foobar message</test_message>\
         <test_values>\
           <test_value name='foo'>foo</test_value>\
           <test_value name='bar'>bar</test_value>\
         </test_values>\
       </example>\
     </examples>\
     <actions>\
       <action>\
         <message>\
           <values>\
             <value name='FOO'>foo</value>\
             <value name='BAR'>bar</value>\
           </values>\
           <tags>\
             <tag>tag1</tag>\
             <tag>tag2</tag>\
           </tags>\
         </message>\
       </action>\
       <action>\
         <create-context context-id='foobar'>\
           <message>\
             <values>\
               <value name='FOO'>foo</value>\
               <value name='BAR'>bar</value>\
             </values>\
             <tags>\
               <tag>tag1</tag>\
               <tag>tag2</tag>\
             </tags>\
             </message>\
         </create-context>\
       </action>\
     </actions>\
    </rule>\
  </rules>\
</ruleset>\
</patterndb>\
";

static void
test_patterndb_loads_a_syntactically_complete_xml_properly(void)
{
  _load_pattern_db_from_string(pdb_complete_syntax);
  /* check we did indeed load the patterns */
  assert_msg_matches_and_has_tag("simple-message", ".classifier.system", TRUE);
  _destroy_pattern_db();
}

gchar *pdb_inheritance_enabled_skeleton = "<patterndb version='4' pub_date='2010-02-22'>\
  <ruleset name='testset' id='1'>\
    <patterns>\
      <pattern>prog2</pattern>\
    </patterns>\
    <rules>\
      <rule provider='test' id='11' class='system'>\
        <patterns>\
          <pattern>pattern-with-inheritance-enabled</pattern>\
        </patterns>\
        <tags>\
          <tag>basetag1</tag>\
          <tag>basetag2</tag>\
        </tags>\
        <actions>\
          <action trigger='match'>\
            <message inherit-properties='TRUE'>\
              <values>\
                <value name='actionkey'>actionvalue</value>\
              </values>\
              <tags>\
                <tag>actiontag</tag>\
              </tags>\
            </message>\
          </action>\
        </actions>\
      </rule>\
    </rules>\
  </ruleset>\
</patterndb>";

void
test_patterndb_message_property_inheritance_enabled(void)
{
  _load_pattern_db_from_string(pdb_inheritance_enabled_skeleton);

  assert_msg_matches_and_output_message_nvpair_equals("pattern-with-inheritance-enabled", 1, "MESSAGE",
                                                      "pattern-with-inheritance-enabled");
  assert_msg_matches_and_output_message_has_tag("pattern-with-inheritance-enabled", 1, "basetag1", TRUE);
  assert_msg_matches_and_output_message_has_tag("pattern-with-inheritance-enabled", 1, "basetag2", TRUE);
  assert_msg_matches_and_output_message_has_tag("pattern-with-inheritance-enabled", 1, "actiontag", TRUE);
  assert_msg_matches_and_output_message_nvpair_equals("pattern-with-inheritance-enabled", 1, "actionkey", "actionvalue");

  _destroy_pattern_db();
}

gchar *pdb_inheritance_disabled_skeleton = "<patterndb version='4' pub_date='2010-02-22'>\
  <ruleset name='testset' id='1'>\
    <patterns>\
      <pattern>prog2</pattern>\
    </patterns>\
    <rules>\
      <rule provider='test' id='12' class='system'>\
        <patterns>\
          <pattern>pattern-with-inheritance-disabled</pattern>\
        </patterns>\
        <tags>\
          <tag>basetag1</tag>\
          <tag>basetag2</tag>\
        </tags>\
        <actions>\
          <action trigger='match'>\
            <message inherit-properties='FALSE'>\
              <values>\
                <value name='actionkey'>actionvalue</value>\
              </values>\
              <tags>\
                <tag>actiontag</tag>\
              </tags>\
            </message>\
          </action>\
        </actions>\
      </rule>\
    </rules>\
 </ruleset>\
</patterndb>";

void
test_patterndb_message_property_inheritance_disabled(void)
{
  _load_pattern_db_from_string(pdb_inheritance_disabled_skeleton);

  assert_msg_matches_and_output_message_nvpair_equals("pattern-with-inheritance-disabled", 1, "MESSAGE", NULL);
  assert_msg_matches_and_output_message_has_tag("pattern-with-inheritance-disabled", 1, "basetag1", FALSE);
  assert_msg_matches_and_output_message_has_tag("pattern-with-inheritance-disabled", 1, "basetag2", FALSE);
  assert_msg_matches_and_output_message_has_tag("pattern-with-inheritance-disabled", 1, "actiontag", TRUE);
  assert_msg_matches_and_output_message_nvpair_equals("pattern-with-inheritance-disabled", 1, "actionkey", "actionvalue");

  _destroy_pattern_db();
}

gchar *pdb_inheritance_context_skeleton = "\
<patterndb version='4' pub_date='2010-02-22'>\
  <ruleset name='testset' id='1'>\
    <patterns>\
      <pattern>prog2</pattern>\
    </patterns>\
    <rules>\
      <rule provider='test' id='11' class='system' context-scope='program'\
           context-id='$PID' context-timeout='60'>\
        <patterns>\
          <pattern>pattern-with-inheritance-context</pattern>\
        </patterns>\
        <tags>\
          <tag>basetag1</tag>\
          <tag>basetag2</tag>\
        </tags>\
        <actions>\
          <action trigger='timeout'>\
            <message inherit-properties='context'>\
              <values>\
                <value name='MESSAGE'>action message</value>\
              </values>\
              <tags>\
                <tag>actiontag</tag>\
              </tags>\
            </message>\
          </action>\
        </actions>\
     </rule>\
    </rules>\
  </ruleset>\
</patterndb>";

void
test_patterndb_message_property_inheritance_context(void)
{
  _load_pattern_db_from_string(pdb_inheritance_context_skeleton);

  _feed_message_to_correllation_state("prog2", "pattern-with-inheritance-context", "merged1", "merged1");
  _feed_message_to_correllation_state("prog2", "pattern-with-inheritance-context", "merged2", "merged2");
  _advance_time(60);

  assert_output_message_nvpair_equals(2, "MESSAGE", "action message");
  assert_output_message_nvpair_equals(2, "merged1", "merged1");
  assert_output_message_nvpair_equals(2, "merged2", "merged2");
  assert_output_message_has_tag(2, "actiontag", TRUE);

  _destroy_pattern_db();
}

void
test_patterndb_message_property_inheritance(void)
{
  test_patterndb_message_property_inheritance_enabled();
  test_patterndb_message_property_inheritance_disabled();
  test_patterndb_message_property_inheritance_context();
}

gchar *pdb_msg_count_skeleton = "<patterndb version='4' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog1</pattern>\
   <pattern>prog2</pattern>\
  </patterns>\
  <rules>\
    <rule provider='test' id='13' class='system' context-scope='program'\
          context-id='$PID' context-timeout='60'>\
      <patterns>\
        <pattern>pattern13</pattern>\
      </patterns>\
      <values>\
        <value name='n13-1'>v13-1</value>\
      </values>\
      <actions>\
        <action condition='\"${n13-1}\" eq \"v13-1\"' trigger='match'>\
          <message inherit-properties='TRUE'>\
            <values>\
              <value name='CONTEXT_LENGTH'>$(context-length)</value>\
            </values>\
          </message>\
        </action>\
      </actions>\
    </rule>\
    <rule provider='test' id='14' class='system' context-scope='program'\
          context-id='$PID' context-timeout='60'>\
      <patterns>\
        <pattern>pattern14</pattern>\
      </patterns>\
      <actions>\
        <action condition='\"$(context-length)\" eq \"1\"' trigger='match'>\
          <message inherit-properties='TRUE'>\
            <values>\
              <value name='CONTEXT_LENGTH'>$(context-length)</value>\
            </values>\
          </message>\
        </action>\
      </actions>\
    </rule>\
    <rule provider='test' id='15' class='system' context-scope='program'\
          context-id='$PID' context-timeout='60'>\
      <patterns>\
        <pattern>pattern15@ANYSTRING:p15@</pattern>\
      </patterns>\
      <actions>\
        <action condition='\"$(context-length)\" eq \"2\"' trigger='match'>\
          <message inherit-properties='FALSE'>\
            <values>\
              <value name='fired'>true</value>\
            </values>\
          </message>\
        </action>\
      </actions>\
    </rule>\
  </rules>\
 </ruleset>\
</patterndb>";

void
test_patterndb_context_length(void)
{
  _load_pattern_db_from_string(pdb_msg_count_skeleton);

  assert_msg_matches_and_output_message_nvpair_equals("pattern13", 1, "CONTEXT_LENGTH", "1");
  assert_msg_matches_and_output_message_nvpair_equals("pattern14", 1, "CONTEXT_LENGTH", "1");

  assert_msg_with_program_matches_and_nvpair_equals("prog2", "pattern15-a", "p15", "-a");

  _dont_reset_patterndb_state_for_the_next_call();
  assert_msg_matches_and_output_message_nvpair_equals("pattern15-b", 2, "fired", "true");

  _destroy_pattern_db();
}

gchar *tag_outside_of_rule_skeleton = "<patterndb version='3' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog1</pattern>\
  </patterns>\
  <tags>\
   <tag>tag1</tag>\
  </tags>\
 </ruleset>\
</patterndb>";

void
test_patterndb_tags_outside_of_rule(void)
{
  patterndb = pattern_db_new();
  messages = NULL;

  g_file_open_tmp("patterndbXXXXXX.xml", &filename, NULL);
  g_file_set_contents(filename, tag_outside_of_rule_skeleton,
                      strlen(tag_outside_of_rule_skeleton), NULL);

  assert_false(pattern_db_reload_ruleset(patterndb, configuration, filename),
               "successfully loaded an invalid patterndb file");
  _destroy_pattern_db();
}

#include "test_parsers_e2e.c"

int
main(int argc, char *argv[])
{

  app_startup();

  msg_init(TRUE);

  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "basicfuncs");
  cfg_load_module(configuration, "syslogformat");

  pattern_db_global_init();

  test_conflicting_rules_with_different_parsers();
  test_conflicting_rules_with_the_same_parsers();
  test_patterndb_rule();
  test_patterndb_loads_a_syntactically_complete_xml_properly();
  test_patterndb_parsers();
  test_patterndb_message_property_inheritance();
  test_patterndb_context_length();
  test_patterndb_tags_outside_of_rule();

  app_shutdown();
  return 0;
}
