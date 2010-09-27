
#include "apphook.h"
#include "tags.h"
#include "logmsg.h"
#include "messages.h"
#include "filter.h"
#include "dbparser/logpatterns.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>

gboolean fail = FALSE;
gboolean verbose = FALSE;

gchar *pdb = "<patterndb version='2' pub_date='2009-07-28'>\
 <ruleset name='testset' id='1'>\
  <rule provider='test' id='11' class='system'>\
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
   </values>\
  </rule>\
  <rule provider='test' id='12' class='violation'>\
   <patterns>\
    <pattern>pattern12</pattern>\
    <pattern>pattern12a</pattern>\
   </patterns>\
  </rule>\
 </ruleset>\
</patterndb>";

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

void
test_rule_value(LogPatternDatabase *patterndb, const gchar *pattern, const gchar *name, const gchar *value)
{
  gboolean result;
  LogMessage *msg = log_msg_new_empty();
  gboolean found = FALSE;
  const gchar *val;
  gssize len;

  log_msg_set_value(msg, LM_V_MESSAGE, pattern, strlen(pattern));
  log_msg_set_value(msg, LM_V_HOST, MYHOST, strlen(MYHOST));

  result = log_pattern_database_lookup(patterndb, msg, NULL);
  val = log_msg_get_value(msg, log_msg_get_value_handle(name), &len);

  if (!!value ^ (len > 0))
    test_fail("Value '%s' is %smatching for pattern '%s' (%d)\n", name, found ? "" : "not ", pattern, !!result);

  log_msg_unref(msg);
}

void
test_rule_tag(LogPatternDatabase *patterndb, const gchar *pattern, const gchar *tag, gboolean set)
{
  LogMessage *msg = log_msg_new_empty();
  gboolean found, result;

  log_msg_set_value(msg, LM_V_MESSAGE, pattern, strlen(pattern));

  result = log_pattern_database_lookup(patterndb, msg, NULL);
  found = log_msg_is_tag_by_name(msg, tag);

  if (set ^ found)
    test_fail("Tag '%s' is %sset for pattern '%s' (%d)\n", tag, found ? "" : "not ", pattern, !!result);

  log_msg_unref(msg);
}

int
main(int argc, char *argv[])
{
  LogPatternDatabase *patterndb;
  gchar *filename = NULL;

  app_startup();

  if (argc > 1)
    verbose = TRUE;

  msg_init(TRUE);
  log_pattern_database_init();

  memset(&patterndb, 0x0, sizeof(LogPatternDatabase));

  g_file_open_tmp("patterndbXXXXXX.xml", &filename, NULL);
  g_file_set_contents(filename, pdb, strlen(pdb), NULL);

  patterndb = log_pattern_database_new();
  if (log_pattern_database_load(patterndb, configuration, filename, NULL))
    {
      if (!g_str_equal(patterndb->version, "2"))
        test_fail("Invalid version '%s'\n", patterndb->version);
      if (!g_str_equal(patterndb->pub_date, "2009-07-28"))
        test_fail("Invalid pub_date '%s'\n", patterndb->pub_date);

      test_rule_tag(patterndb, "pattern11", "tag11-1", TRUE);
      test_rule_tag(patterndb, "pattern11", ".classifier.system", TRUE);
      test_rule_tag(patterndb, "pattern11", "tag11-2", TRUE);
      test_rule_tag(patterndb, "pattern11", "tag11-3", FALSE);
      test_rule_tag(patterndb, "pattern11a", "tag11-1", TRUE);
      test_rule_tag(patterndb, "pattern11a", "tag11-2", TRUE);
      test_rule_tag(patterndb, "pattern11a", "tag11-3", FALSE);
      test_rule_tag(patterndb, "pattern12", ".classifier.violation", TRUE);
      test_rule_tag(patterndb, "pattern12", "tag12-1", FALSE);
      test_rule_tag(patterndb, "pattern12", "tag12-2", FALSE);
      test_rule_tag(patterndb, "pattern12", "tag12-3", FALSE);
      test_rule_tag(patterndb, "pattern12a", "tag12-1", FALSE);
      test_rule_tag(patterndb, "pattern12a", "tag12-2", FALSE);
      test_rule_tag(patterndb, "pattern12a", "tag12-3", FALSE);
      test_rule_tag(patterndb, "pattern1x", "tag1x-1", FALSE);
      test_rule_tag(patterndb, "pattern1x", "tag1x-2", FALSE);
      test_rule_tag(patterndb, "pattern1x", "tag1x-3", FALSE);
      test_rule_tag(patterndb, "pattern1xa", "tag1x-1", FALSE);
      test_rule_tag(patterndb, "pattern1xa", "tag1x-2", FALSE);
      test_rule_tag(patterndb, "pattern1xa", "tag1x-3", FALSE);

      test_rule_value(patterndb, "pattern11", "n11-1", "v11-1");
      test_rule_value(patterndb, "pattern11", ".classifier.class", "system");
      test_rule_value(patterndb, "pattern11", "n11-2", "v11-2");
      test_rule_value(patterndb, "pattern11", "n11-3", NULL);
      test_rule_value(patterndb, "pattern11a", "n11-1", "v11-1");
      test_rule_value(patterndb, "pattern11a", "n11-2", "v11-2");
      test_rule_value(patterndb, "pattern11a", "n11-3", NULL);
      test_rule_value(patterndb, "pattern12", ".classifier.class", "violation");
      test_rule_value(patterndb, "pattern12", "n12-1", NULL);
      test_rule_value(patterndb, "pattern12", "n12-2", NULL);
      test_rule_value(patterndb, "pattern12", "n12-3", NULL);
      test_rule_value(patterndb, "pattern1x", "n1x-1", NULL);
      test_rule_value(patterndb, "pattern1x", "n1x-2", NULL);
      test_rule_value(patterndb, "pattern1x", "n1x-3", NULL);

      test_rule_value(patterndb, "pattern11", "vvv", MYHOST);

      log_pattern_database_free(patterndb);
    }
  else
    fail = TRUE;

  g_unlink(filename);
  g_free(filename);
  app_shutdown();
  return  (fail ? 1 : 0);
}
