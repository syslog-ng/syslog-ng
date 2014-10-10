#include "apphook.h"
#include "tags.h"
#include "logmsg.h"
#include "messages.h"
#include "filter/filter-expr.h"
#include "patterndb.h"
#include "plugin.h"
#include "cfg.h"
#include "patterndb-int.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>

gboolean fail = FALSE;
gboolean verbose = FALSE;

#define test_fail(fmt, args...) \
do {\
  printf(fmt, ##args); \
  fail = TRUE; \
} while (0);

#define test_msg(fmt, args...) \
do { \
  if (verbose) printf(fmt, ##args); \
} while (0);

#define MYHOST "MYHOST"
#define MYPID "999"

PatternDB *patterndb;
gchar *filename;
GPtrArray *messages;

void
test_emit_func(LogMessage *msg, gboolean synthetic, gpointer user_data)
{
  g_ptr_array_add(messages, log_msg_ref(msg));
}

void
create_pattern_db(gchar *pdb)
{
  patterndb = pattern_db_new();
  messages = g_ptr_array_new();

  pattern_db_set_emit_func(patterndb, test_emit_func, NULL);

  g_file_open_tmp("patterndbXXXXXX.xml", &filename, NULL);
  g_file_set_contents(filename, pdb, strlen(pdb), NULL);

  if (pattern_db_reload_ruleset(patterndb, configuration, filename))
    {
      if (!g_str_equal(pattern_db_get_ruleset_version(patterndb), "3"))
        test_fail("Invalid version '%s'\n", pattern_db_get_ruleset_version(patterndb));
      if (!g_str_equal(pattern_db_get_ruleset_pub_date(patterndb), "2010-02-22"))
        test_fail("Invalid pub_date '%s'\n", pattern_db_get_ruleset_pub_date(patterndb));
    }
  else
    {
      fail = TRUE;
    }
}

void
clean_pattern_db(void)
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
test_clean_state(void)
{
  pattern_db_forget_state(patterndb);
  g_ptr_array_foreach(messages, (GFunc) log_msg_unref, NULL);
  g_ptr_array_set_size(messages, 0);
}

void
test_rule_value_without_clean(const gchar *program, const gchar *pattern,
                              const gchar *name, const gchar *value)
{
  gboolean result;
  LogMessage *msg = log_msg_new_empty();
  gboolean found = FALSE;
  const gchar *val;
  gssize len;
  PDBInput input;

  log_msg_set_value(msg, LM_V_MESSAGE, pattern, strlen(pattern));
  log_msg_set_value(msg, LM_V_PROGRAM, program, 5);
  log_msg_set_value(msg, LM_V_HOST, MYHOST, strlen(MYHOST));
  log_msg_set_value(msg, LM_V_PID, MYPID, strlen(MYPID));

  result = pattern_db_process(patterndb, PDB_INPUT_WRAP_MESSAGE(&input, msg));
  val = log_msg_get_value_by_name(msg, name, &len);
  if (value)
    found = strcmp(val, value) == 0;

  if (!!value ^ (len > 0))
    test_fail("Value '%s' is %smatching for pattern '%s' (%d)\n", name, found ? "" : "not ", pattern, !!result);

  log_msg_unref(msg);
}

void
test_rule_value(const gchar *pattern, const gchar *name, const gchar *value)
{
  test_rule_value_without_clean("prog1", pattern, name, value);
  test_clean_state();
}

void
test_rule_tag(const gchar *pattern, const gchar *tag, gboolean set)
{
  LogMessage *msg = log_msg_new_empty();
  gboolean found, result;
  PDBInput input;

  log_msg_set_value(msg, LM_V_MESSAGE, pattern, strlen(pattern));
  log_msg_set_value(msg, LM_V_PROGRAM, "prog2", 5);
  log_msg_set_value(msg, LM_V_HOST, MYHOST, strlen(MYHOST));
  log_msg_set_value(msg, LM_V_PID, MYPID, strlen(MYPID));

  result = pattern_db_process(patterndb, PDB_INPUT_WRAP_MESSAGE(&input, msg));
  found = log_msg_is_tag_by_name(msg, tag);

  if (set ^ found)
    test_fail("Tag '%s' is %sset for pattern '%s' (%d)\n", tag, found ? "" : "not ", pattern, !!result);

  log_msg_unref(msg);
  test_clean_state();
}

void
test_rule_action_message_value(const gchar *pattern, gint timeout, gint ndx, const gchar *name, const gchar *value)
{
  LogMessage *msg = log_msg_new_empty();
  gboolean found = FALSE, result;
  const gchar *val;
  gssize len;
  PDBInput input;

  log_msg_set_value(msg, LM_V_MESSAGE, pattern, strlen(pattern));
  log_msg_set_value(msg, LM_V_PROGRAM, "prog2", 5);
  log_msg_set_value(msg, LM_V_HOST, MYHOST, strlen(MYHOST));
  log_msg_set_value(msg, LM_V_PID, MYPID, strlen(MYPID));
  msg->timestamps[LM_TS_STAMP].tv_sec = msg->timestamps[LM_TS_RECVD].tv_sec;

  result = pattern_db_process(patterndb, PDB_INPUT_WRAP_MESSAGE(&input, msg));
  if (timeout)
    timer_wheel_set_time(patterndb->timer_wheel, timer_wheel_get_time(patterndb->timer_wheel) + timeout + 1);

  if (ndx >= messages->len)
    {
      test_fail("Expected the %d. message, but no such message was returned by patterndb\n", ndx);
      goto exit;
    }

  val = log_msg_get_value_by_name((LogMessage *) g_ptr_array_index(messages, ndx), name, &len);
  if (value)
    found = strcmp(val, value) == 0;

  if (((value == NULL) && (len != NULL)) || 
      ((value != NULL) && ( (len == NULL) || (!found) )))
    test_fail("Value '%s' is %smatching for pattern '%s' (%d) index %d\n", name, found ? "" : "not ", pattern, !!result, ndx);

exit:
  log_msg_unref(msg);
  test_clean_state();
}

void
test_rule_action_message_tag(const gchar *pattern, gint timeout, gint ndx, const gchar *tag, gboolean set)
{
  LogMessage *msg = log_msg_new_empty();
  gboolean found, result;
  PDBInput input;

  log_msg_set_value(msg, LM_V_MESSAGE, pattern, strlen(pattern));
  log_msg_set_value(msg, LM_V_PROGRAM, "prog2", 5);
  log_msg_set_value(msg, LM_V_HOST, MYHOST, strlen(MYHOST));
  log_msg_set_value(msg, LM_V_PID, MYPID, strlen(MYPID));
  msg->timestamps[LM_TS_STAMP].tv_sec = msg->timestamps[LM_TS_RECVD].tv_sec;

  result = pattern_db_process(patterndb, PDB_INPUT_WRAP_MESSAGE(&input, msg));
  if (timeout)
    timer_wheel_set_time(patterndb->timer_wheel, timer_wheel_get_time(patterndb->timer_wheel) + timeout + 5);
  if (ndx >= messages->len)
    {
      test_fail("Expected the %d. message, but no such message was returned by patterndb\n", ndx);
      goto exit;
    }
  found = log_msg_is_tag_by_name((LogMessage *) g_ptr_array_index(messages, ndx), tag);

  if (set ^ found)
    test_fail("Tag '%s' is %sset for pattern '%s' (%d), index %d\n", tag, found ? "" : "not ", pattern, !!result, ndx);
exit:
  log_msg_unref(msg);
  test_clean_state();
}


void
test_patterndb_rule(void)
{
  create_pattern_db(pdb_ruletest_skeleton);
  test_rule_tag("pattern11", "tag11-1", TRUE);
  test_rule_tag("pattern11", ".classifier.system", TRUE);
  test_rule_tag("pattern11", "tag11-2", TRUE);
  test_rule_tag("pattern11", "tag11-3", FALSE);
  test_rule_tag("pattern11a", "tag11-1", TRUE);
  test_rule_tag("pattern11a", "tag11-2", TRUE);
  test_rule_tag("pattern11a", "tag11-3", FALSE);
  test_rule_tag("pattern12", ".classifier.violation", TRUE);
  test_rule_tag("pattern12", "tag12-1", FALSE);
  test_rule_tag("pattern12", "tag12-2", FALSE);
  test_rule_tag("pattern12", "tag12-3", FALSE);
  test_rule_tag("pattern12a", "tag12-1", FALSE);
  test_rule_tag("pattern12a", "tag12-2", FALSE);
  test_rule_tag("pattern12a", "tag12-3", FALSE);
  test_rule_tag("pattern1x", "tag1x-1", FALSE);
  test_rule_tag("pattern1x", "tag1x-2", FALSE);
  test_rule_tag("pattern1x", "tag1x-3", FALSE);
  test_rule_tag("pattern1xa", "tag1x-1", FALSE);
  test_rule_tag("pattern1xa", "tag1x-2", FALSE);
  test_rule_tag("pattern1xa", "tag1x-3", FALSE);
  test_rule_tag("foobar", ".classifier.unknown", TRUE);

  test_rule_value("pattern11", "n11-1", "v11-1");
  test_rule_value("pattern11", ".classifier.class", "system");
  test_rule_value("pattern11", "n11-2", "v11-2");
  test_rule_value("pattern11", "n11-3", NULL);
  test_rule_value("pattern11", "context-id", "999");
  test_rule_value("pattern11", ".classifier.context_id", "999");
  test_rule_value("pattern11a", "n11-1", "v11-1");
  test_rule_value("pattern11a", "n11-2", "v11-2");
  test_rule_value("pattern11a", "n11-3", NULL);
  test_rule_value("pattern12", ".classifier.class", "violation");
  test_rule_value("pattern12", "n12-1", NULL);
  test_rule_value("pattern12", "n12-2", NULL);
  test_rule_value("pattern12", "n12-3", NULL);
  test_rule_value("pattern1x", "n1x-1", NULL);
  test_rule_value("pattern1x", "n1x-2", NULL);
  test_rule_value("pattern1x", "n1x-3", NULL);
  test_rule_value("pattern11", "vvv", MYHOST);

  test_rule_action_message_value("pattern11", 0, 1, "MESSAGE", "rule11 matched");
  test_rule_action_message_value("pattern11", 0, 1, "context-id", "999");
  test_rule_action_message_tag("pattern11", 0, 1, "tag11-3", TRUE);
  test_rule_action_message_tag("pattern11", 0, 1, "tag11-4", FALSE);

  test_rule_action_message_value("pattern11", 60, 2, "MESSAGE", "rule11 timed out");
  test_rule_action_message_value("pattern11", 60, 2, "context-id", "999");
  test_rule_action_message_tag("pattern11", 60, 2, "tag11-3", FALSE);
  test_rule_action_message_tag("pattern11", 60, 2, "tag11-4", TRUE);

  test_rule_action_message_value("contextlesstest value1", 0, 1, "MESSAGE",  "message1");
  test_rule_action_message_value("contextlesstest value2", 0, 1, "MESSAGE",  "message2");

  clean_pattern_db();
}

gchar *pdb_parser_skeleton_prefix ="<?xml version='1.0' encoding='UTF-8'?>\
          <patterndb version='3' pub_date='2010-02-22'>\
            <ruleset name='test1_program' id='480de478-d4a6-4a7f-bea4-0c0245d361e1'>\
                <pattern>test</pattern>\
                    <rule id='1' class='test1' provider='my'>\
                        <patterns>\
                         <pattern>";

gchar *pdb_parser_skeleton_postfix =  "</pattern>\
                      </patterns>\
                    </rule>\
            </ruleset>\
        </patterndb>";


void
test_pattern(const gchar *pattern, const gchar *rule, gboolean match)
{
  gboolean result;
  LogMessage *msg = log_msg_new_empty();
  static LogTemplate *templ;

  GString *res = g_string_sized_new(128);
  static TimeZoneInfo *tzinfo = NULL;

  PDBInput input;

  if (!tzinfo)
    tzinfo = time_zone_info_new(NULL);
  if (!templ)
    {
      templ = log_template_new(configuration, "dummy");
      log_template_compile(templ, "$TEST", NULL);
    }

  log_msg_set_value(msg, LM_V_HOST, MYHOST, strlen(MYHOST));
  log_msg_set_value(msg, LM_V_PROGRAM, "test", strlen(MYHOST));
  log_msg_set_value(msg, LM_V_MESSAGE, pattern, strlen(pattern));

  result = pattern_db_process(patterndb, PDB_INPUT_WRAP_MESSAGE(&input, msg));

  log_template_format(templ, msg, NULL, LTZ_LOCAL, 0, NULL, res);

  if (strcmp(res->str, pattern) == 0)
    {
       test_msg("Rule: '%s' Value '%s' is inserted into $TEST res:(%s)\n", rule, pattern, res->str);
    }

  if ((match && !result) || (!match && result))
     {
       test_fail("FAIL: Value '%s' is %smatching for pattern '%s' \n", rule, !!result ? "" : "not ", pattern);
     }
   else
    {
       test_msg("Value '%s' is %smatching for pattern '%s' \n", rule, !!result ? "" : "not ", pattern);
     }

  g_string_free(res, TRUE);

  log_msg_unref(msg);
}

void
test_parser(gchar **test)
{
  GString *str;
  gint index = 1;

  str = g_string_new(pdb_parser_skeleton_prefix);
  g_string_append(str, test[0]);
  g_string_append(str, pdb_parser_skeleton_postfix);

  create_pattern_db(str->str);
  g_string_free(str, TRUE);
  while(test[index] != NULL)
    test_pattern(test[index++], test[0], TRUE);
  while(test[index] != NULL)
    test_pattern(test[index++], test[0], FALSE);

  clean_pattern_db();
}

gchar * test1 [] = {
"@ANYSTRING:TEST@",
"ab ba ab",
"ab ba ab",
"1234ab",
"ab1234",
"1.2.3.4",
"ab  1234  ba",
"&lt;ab ba&gt;",
NULL,NULL
};

gchar * test2 [] = {
"@DOUBLE:TEST@",
"1234",
"1234.567",
"1.2.3.4",
"1234ab",
NULL, // not match
"ab1234",NULL
};

gchar * test3 [] = {
"@ESTRING:TEST:endmark@",
"ab ba endmark",
NULL,
"ab ba",NULL
};

gchar * test4 [] = {
"@ESTRING:TEST:&gt;@",
"ab ba > ab",
NULL,
"ab ba",NULL
};

gchar * test5 [] = {
"@FLOAT:TEST@",
"1234",
"1234.567",
"1.2.3.4",
"1234ab",
NULL, // not match
"ab1234",NULL
};

gchar * test6 [] = {
"@SET:TEST: 	@",
" a ",
"  a ",
" 	a ",
" 	 a ",
NULL, // not match
"ab1234",NULL
};

gchar * test7 [] = {
"@IPv4:TEST@",
"1.2.3.4",
"0.0.0.0",
"255.255.255.255",
NULL,
"256.256.256.256",
"1.2.3.4.5",
"1234",
"ab1234",
"ab1.2.3.4",
"1,2,3,4",NULL
};

gchar * test8 [] = {
"@IPv6:TEST@",
"2001:0db8:0000:0000:0000:0000:1428:57ab",
"2001:0db8:0000:0000:0000::1428:57ab",
"2001:0db8:0:0:0:0:1428:57ab",
"2001:0db8:0:0::1428:57ab",
"2001:0db8::1428:57ab",
"2001:db8::1428:57ab",
NULL,
"2001:0db8::34d2::1428:57ab",NULL
};

gchar * test9 [] = {
"@IPvANY:TEST@",
"1.2.3.4",
"0.0.0.0",
"255.255.255.255",
"2001:0db8:0000:0000:0000:0000:1428:57ab",
"2001:0db8:0000:0000:0000::1428:57ab",
"2001:0db8:0:0:0:0:1428:57ab",
"2001:0db8:0:0::1428:57ab",
"2001:0db8::1428:57ab",
"2001:db8::1428:57ab",
NULL,
"256.256.256.256",
"1.2.3.4.5",
"1234",
"ab1234",
"ab1.2.3.4",
"1,2,3,4",
"2001:0db8::34d2::1428:57ab",NULL
};

gchar * test10 [] = {
"@NUMBER:TEST@",
"1234",
"1.2",
"1.2.3.4",
"1234ab",
NULL,
"ab1234",
"1,2",NULL
};

gchar * test11 [] = {
"@QSTRING:TEST:&lt;&gt;@",
"<aa bb>",
"< aabb >",
NULL,
"aabb>",
"<aabb",NULL
};

gchar * test12 [] = {
"@STRING:TEST@",
"aabb",
"aa bb",
"1234",
"ab1234",
"1234bb",
"1.2.3.4",
NULL,
"aa bb",NULL
};

gchar **parsers[] = {test1, test2, test3, test4, test5, test6, test7, test8, test9, test10, test11, test12, NULL};

void
test_patterndb_parsers()
{
  gint i;

  for (i = 0; parsers[i]; i++)
    {
      test_parser(parsers[i]);
    }
}

gchar *pdb_inheritance_skeleton = "<patterndb version='3' pub_date='2010-02-22'>\
 <ruleset name='testset' id='1'>\
  <patterns>\
   <pattern>prog2</pattern>\
  </patterns>\
  <rule provider='test' id='11' class='system' context-scope='program'\
        context-id='$PID' context-timeout='60'>\
   <patterns>\
    <pattern>pattern11</pattern>\
   </patterns>\
   <tags>\
    <tag>tag11-1</tag>\
    <tag>tag11-2</tag>\
   </tags>\
   <values>\
    <value name='n11-1'>v11-1</value>\
    <value name='vvv'>${HOST}</value>\
    <value name='context-id'>${CONTEXT_ID}</value>\
   </values>\
   <actions>\
    <action rate='1/60' condition='\"${n11-1}\" == \"v11-1\"' trigger='match'>\
     <message inherit-properties='TRUE'>\
      <value name='context-id'>${CONTEXT_ID}</value>\
      <tags>\
       <tag>tag11-3</tag>\
      </tags>\
     </message>\
    </action>\
   </actions>\
  </rule>\
  <rule provider='test' id='12' class='system' context-scope='program'\
        context-id='$PID' context-timeout='60'>\
   <patterns>\
    <pattern>pattern12</pattern>\
   </patterns>\
   <tags>\
    <tag>tag12-1</tag>\
    <tag>tag12-2</tag>\
   </tags>\
   <values>\
    <value name='n12-1'>v12-1</value>\
    <value name='vvv'>${HOST}</value>\
    <value name='context-id'>${CONTEXT_ID}</value>\
   </values>\
   <actions>\
    <action rate='1/60' condition='\"${n12-1}\" == \"v12-1\"' trigger='match'>\
     <message inherit-properties='FALSE'>\
      <value name='context-id'>${CONTEXT_ID}</value>\
      <tags>\
       <tag>tag12-3</tag>\
      </tags>\
     </message>\
    </action>\
   </actions>\
  </rule>\
 </ruleset>\
</patterndb>";

void
test_patterndb_message_property_inheritance()
{
  create_pattern_db(pdb_inheritance_skeleton);

  test_rule_action_message_value("pattern11", 0, 1, "MESSAGE", "pattern11");
  test_rule_action_message_value("pattern12", 0, 1, "MESSAGE", NULL);

  test_rule_action_message_tag("pattern11", 0, 1, "tag11-1", TRUE);
  test_rule_action_message_tag("pattern11", 0, 1, "tag11-2", TRUE);
  test_rule_action_message_tag("pattern11", 0, 1, "tag11-3", TRUE);

  test_rule_action_message_tag("pattern12", 0, 1, "tag12-1", FALSE);
  test_rule_action_message_tag("pattern12", 0, 1, "tag12-2", FALSE);
  test_rule_action_message_tag("pattern12", 0, 1, "tag12-3", TRUE);

  clean_pattern_db();
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
  create_pattern_db(pdb_msg_count_skeleton);

  test_rule_action_message_value("pattern13", 0, 1, "CONTEXT_LENGTH", "1");
  test_rule_action_message_value("pattern14", 0, 1, "CONTEXT_LENGTH", "1");

  test_rule_value_without_clean("prog2", "pattern15-a", "p15", "-a");
  test_rule_action_message_value("pattern15-b", 0, 2, "fired", "true");

  clean_pattern_db();
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

  if (pattern_db_reload_ruleset(patterndb, configuration, filename))
    fail = TRUE;

  clean_pattern_db();
}

int
main(int argc, char *argv[])
{

  app_startup();

  if (argc > 1)
    verbose = TRUE;

  msg_init(TRUE);

  configuration = cfg_new(0x0302);
  plugin_load_module("basicfuncs", configuration, NULL);
  plugin_load_module("syslogformat", configuration, NULL);

  pattern_db_global_init();

  test_patterndb_rule();
  test_patterndb_parsers();
  test_patterndb_message_property_inheritance();
  test_patterndb_context_length();
  test_patterndb_tags_outside_of_rule();

  app_shutdown();
  return  (fail ? 1 : 0);
}
