#include "apphook.h"
#include "tags.h"
#include "logmsg.h"
#include "messages.h"
#include "filter/filter-expr.h"
#include "patterndb.h"
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
_load_pattern_db_from_string(gchar *pdb)
{
  patterndb = pattern_db_new();
  messages = g_ptr_array_new();

  pattern_db_set_emit_func(patterndb, _emit_func, NULL);

  g_file_open_tmp("patterndbXXXXXX.xml", &filename, NULL);
  g_file_set_contents(filename, pdb, strlen(pdb), NULL);

  assert_true(pattern_db_reload_ruleset(patterndb, configuration, filename), "Error loading ruleset [[[%s]]]", pdb);
  assert_string(pattern_db_get_ruleset_version(patterndb), "3", "Invalid version");
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
    timer_wheel_set_time(pattern_db_get_timer_wheel(patterndb), timer_wheel_get_time(pattern_db_get_timer_wheel(patterndb)) + timeout + 1);
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
assert_msg_matches_and_output_message_nvpair_equals_with_timeout(const gchar *pattern, gint timeout, gint ndx, const gchar *name, const gchar *value)
{
  LogMessage *msg;

  msg = _construct_message("prog2", pattern);
  _process(msg);
  _advance_time(timeout);

  assert_output_message_nvpair_equals(ndx, name, value);
  log_msg_unref(msg);
}

void
assert_msg_matches_and_output_message_nvpair_equals(const gchar *pattern, gint ndx, const gchar *name, const gchar *value)
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
assert_msg_matches_and_output_message_has_tag_with_timeout(const gchar *pattern, gint timeout, gint ndx, const gchar *tag, gboolean set)
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

gchar *pdb_conflicting_rules_with_different_parsers = "<patterndb version='3' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog1</pattern>\
   <pattern>prog2</pattern>\
  </patterns>\
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

gchar *pdb_conflicting_rules_with_the_same_parsers = "<patterndb version='3' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog1</pattern>\
   <pattern>prog2</pattern>\
  </patterns>\
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
gchar *pdb_ruletest_skeleton = "<patterndb version='3' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
    <pattern>prog1</pattern>\
    <pattern>prog2</pattern>\
  </patterns>\
  <rule provider='test' id='11' class='system' context-scope='program' context-id='$PID' context-timeout='60'>\
   <patterns>\
    <pattern>pattern11</pattern>\
    <pattern>pattern11a</pattern>\
   </patterns>\
   <tags>\
    <tag>tag11-1</tag>\
    <tag>tag11-2</tag>\
   </tags>\
   <values>\
    <value name='n11-1'>v11-1</value>\
    <value name='n11-2'>v11-2</value>\
    <value name='vvv'>${HOST}</value>\
    <value name='context-id'>${CONTEXT_ID}</value>\
   </values>\
   <actions>\
     <action rate='1/60' condition='\"${n11-1}\" == \"v11-1\"' trigger='match'>\
       <message>\
         <value name='MESSAGE'>rule11 matched</value>\
         <value name='context-id'>${CONTEXT_ID}</value>\
         <tags>\
           <tag>tag11-3</tag>\
         </tags>\
       </message>\
     </action>\
     <action rate='1/60' condition='\"${n11-1}\" == \"v11-1\"' trigger='timeout'>\
       <message>\
         <value name='MESSAGE'>rule11 timed out</value>\
         <value name='context-id'>${CONTEXT_ID}</value>\
         <tags>\
           <tag>tag11-4</tag>\
         </tags>\
       </message>\
     </action>\
   </actions>\
  </rule>\
  <rule provider='test' id='12' class='violation'>\
   <patterns>\
    <pattern>pattern12</pattern>\
    <pattern>pattern12a</pattern>\
   </patterns>\
  </rule>\
  <rule provider='test' id='11' class='system'>\
   <patterns>\
    <pattern>contextlesstest @STRING:field:@</pattern>\
   </patterns>\
   <actions>\
     <action condition='\"${field}\" == \"value1\"'>\
       <message>\
         <value name='MESSAGE'>message1</value>\
       </message>\
     </action>\
     <action condition='\"${field}\" == \"value2\"'>\
       <message>\
         <value name='MESSAGE'>message2</value>\
       </message>\
     </action>\
   </actions>\
  </rule>\
 </ruleset>\
</patterndb>";

void
test_patterndb_rule(void)
{
  _load_pattern_db_from_string(pdb_ruletest_skeleton);
  assert_msg_matches_and_has_tag("pattern11", "tag11-1", TRUE);
  assert_msg_matches_and_has_tag("pattern11", ".classifier.system", TRUE);
  assert_msg_matches_and_has_tag("pattern11", "tag11-2", TRUE);
  assert_msg_matches_and_has_tag("pattern11", "tag11-3", FALSE);
  assert_msg_matches_and_has_tag("pattern11a", "tag11-1", TRUE);
  assert_msg_matches_and_has_tag("pattern11a", "tag11-2", TRUE);
  assert_msg_matches_and_has_tag("pattern11a", "tag11-3", FALSE);
  assert_msg_matches_and_has_tag("pattern12", ".classifier.violation", TRUE);
  assert_msg_matches_and_has_tag("pattern12", "tag12-1", FALSE);
  assert_msg_matches_and_has_tag("pattern12", "tag12-2", FALSE);
  assert_msg_matches_and_has_tag("pattern12", "tag12-3", FALSE);
  assert_msg_matches_and_has_tag("pattern12a", "tag12-1", FALSE);
  assert_msg_matches_and_has_tag("pattern12a", "tag12-2", FALSE);
  assert_msg_matches_and_has_tag("pattern12a", "tag12-3", FALSE);
  assert_msg_doesnot_match("pattern1x");

  assert_msg_matches_and_nvpair_equals("pattern11", "n11-1", "v11-1");
  assert_msg_matches_and_nvpair_equals("pattern11", ".classifier.class", "system");
  assert_msg_matches_and_nvpair_equals("pattern11", "n11-2", "v11-2");
  assert_msg_matches_and_nvpair_equals("pattern11", "n11-3", NULL);
  assert_msg_matches_and_nvpair_equals("pattern11", "context-id", "999");
  assert_msg_matches_and_nvpair_equals("pattern11", ".classifier.context_id", "999");
  assert_msg_matches_and_nvpair_equals("pattern11a", "n11-1", "v11-1");
  assert_msg_matches_and_nvpair_equals("pattern11a", "n11-2", "v11-2");
  assert_msg_matches_and_nvpair_equals("pattern11a", "n11-3", NULL);
  assert_msg_matches_and_nvpair_equals("pattern12", ".classifier.class", "violation");
  assert_msg_matches_and_nvpair_equals("pattern12", "n12-1", NULL);
  assert_msg_matches_and_nvpair_equals("pattern12", "n12-2", NULL);
  assert_msg_matches_and_nvpair_equals("pattern12", "n12-3", NULL);
  assert_msg_matches_and_nvpair_equals("pattern11", "vvv", MYHOST);

  assert_msg_matches_and_output_message_nvpair_equals("pattern11", 1, "MESSAGE", "rule11 matched");
  assert_msg_matches_and_output_message_nvpair_equals("pattern11", 1, "context-id", "999");
  assert_msg_matches_and_output_message_has_tag("pattern11", 1, "tag11-3", TRUE);
  assert_msg_matches_and_output_message_has_tag("pattern11", 1, "tag11-4", FALSE);

  assert_msg_matches_and_output_message_nvpair_equals_with_timeout("pattern11", 60, 2, "MESSAGE", "rule11 timed out");
  assert_msg_matches_and_output_message_nvpair_equals_with_timeout("pattern11", 60, 2, "context-id", "999");
  assert_msg_matches_and_output_message_has_tag_with_timeout("pattern11", 60, 2, "tag11-3", FALSE);
  assert_msg_matches_and_output_message_has_tag_with_timeout("pattern11", 60, 2, "tag11-4", TRUE);

  assert_msg_matches_and_output_message_nvpair_equals("contextlesstest value1", 1, "MESSAGE",  "message1");
  assert_msg_matches_and_output_message_nvpair_equals("contextlesstest value2", 1, "MESSAGE",  "message2");

  _destroy_pattern_db();
}

gchar *pdb_inheritance_enabled_skeleton = "<patterndb version='3' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog2</pattern>\
  </patterns>\
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
      <value name='actionkey'>actionvalue</value>\
      <tags>\
       <tag>actiontag</tag>\
      </tags>\
     </message>\
    </action>\
   </actions>\
  </rule>\
 </ruleset>\
</patterndb>";

void
test_patterndb_message_property_inheritance_enabled()
{
  _load_pattern_db_from_string(pdb_inheritance_enabled_skeleton);

  assert_msg_matches_and_output_message_nvpair_equals("pattern-with-inheritance-enabled", 1, "MESSAGE", "pattern-with-inheritance-enabled");
  assert_msg_matches_and_output_message_has_tag("pattern-with-inheritance-enabled", 1, "basetag1", TRUE);
  assert_msg_matches_and_output_message_has_tag("pattern-with-inheritance-enabled", 1, "basetag2", TRUE);
  assert_msg_matches_and_output_message_has_tag("pattern-with-inheritance-enabled", 1, "actiontag", TRUE);
  assert_msg_matches_and_output_message_nvpair_equals("pattern-with-inheritance-enabled", 1, "actionkey", "actionvalue");

  _destroy_pattern_db();
}

gchar *pdb_inheritance_disabled_skeleton = "<patterndb version='3' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog2</pattern>\
  </patterns>\
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
      <value name='actionkey'>actionvalue</value>\
      <tags>\
       <tag>actiontag</tag>\
      </tags>\
     </message>\
    </action>\
   </actions>\
  </rule>\
 </ruleset>\
</patterndb>";

void
test_patterndb_message_property_inheritance_disabled()
{
  _load_pattern_db_from_string(pdb_inheritance_disabled_skeleton);

  assert_msg_matches_and_output_message_nvpair_equals("pattern-with-inheritance-disabled", 1, "MESSAGE", NULL);
  assert_msg_matches_and_output_message_has_tag("pattern-with-inheritance-disabled", 1, "basetag1", FALSE);
  assert_msg_matches_and_output_message_has_tag("pattern-with-inheritance-disabled", 1, "basetag2", FALSE);
  assert_msg_matches_and_output_message_has_tag("pattern-with-inheritance-disabled", 1, "actiontag", TRUE);
  assert_msg_matches_and_output_message_nvpair_equals("pattern-with-inheritance-disabled", 1, "actionkey", "actionvalue");

  _destroy_pattern_db();
}

gchar *pdb_inheritance_context_skeleton = "<patterndb version='3' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog2</pattern>\
  </patterns>\
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
      <value name='MESSAGE'>action message</value>\
      <tags>\
       <tag>actiontag</tag>\
      </tags>\
     </message>\
    </action>\
   </actions>\
  </rule>\
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

gchar *pdb_msg_count_skeleton = "<patterndb version='3' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog1</pattern>\
   <pattern>prog2</pattern>\
  </patterns>\
  <rule provider='test' id='13' class='system' context-scope='program'\
        context-id='$PID' context-timeout='60'>\
   <patterns>\
    <pattern>pattern13</pattern>\
   </patterns>\
   <values>\
    <value name='n13-1'>v13-1</value>\
   </values>\
   <actions>\
    <action condition='\"${n13-1}\" == \"v13-1\"' trigger='match'>\
     <message inherit-properties='TRUE'>\
      <value name='CONTEXT_LENGTH'>$(context-length)</value>\
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
    <action condition='\"$(context-length)\" == \"1\"' trigger='match'>\
     <message inherit-properties='TRUE'>\
      <value name='CONTEXT_LENGTH'>$(context-length)</value>\
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
    <action condition='\"$(context-length)\" == \"2\"' trigger='match'>\
     <message inherit-properties='FALSE'>\
      <value name='fired'>true</value>\
     </message>\
    </action>\
   </actions>\
  </rule>\
 </ruleset>\
</patterndb>";

void
test_patterndb_context_length()
{
  _load_pattern_db_from_string(pdb_msg_count_skeleton);

  assert_msg_matches_and_output_message_nvpair_equals("pattern13", 1, "CONTEXT_LENGTH", "2");
  assert_msg_matches_and_output_message_nvpair_equals("pattern14", 1, "CONTEXT_LENGTH", "2");

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
test_patterndb_tags_outside_of_rule()
{
  patterndb = pattern_db_new();
  messages = NULL;

  g_file_open_tmp("patterndbXXXXXX.xml", &filename, NULL);
  g_file_set_contents(filename, tag_outside_of_rule_skeleton,
                      strlen(tag_outside_of_rule_skeleton), NULL);

  assert_false(pattern_db_reload_ruleset(patterndb, configuration, filename), "successfully loaded an invalid patterndb file");
  _destroy_pattern_db();
}

#include "test_parsers_e2e.c"

int
main(int argc, char *argv[])
{

  app_startup();

  msg_init(TRUE);

  configuration = cfg_new(0x0302);
  plugin_load_module("basicfuncs", configuration, NULL);
  plugin_load_module("syslogformat", configuration, NULL);

  pattern_db_global_init();

  test_conflicting_rules_with_different_parsers();
  test_conflicting_rules_with_the_same_parsers();
  test_patterndb_rule();
  test_patterndb_parsers();
  test_patterndb_message_property_inheritance();
  test_patterndb_context_length();
  test_patterndb_tags_outside_of_rule();

  app_shutdown();
  return 0;
}
