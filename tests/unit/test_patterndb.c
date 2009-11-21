
#include "apphook.h"
#include "tags.h"
#include "logmsg.h"
#include "messages.h"
#include "filter.h"
#include "logpatterns.h"

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
  <rule provider='test' id='12' class='system'>\
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
  LogDBResult *result;
  LogMessage *msg = log_msg_new_empty();
  gboolean found = FALSE;
  GString *val = g_string_sized_new(256);
  gint i = 0;

  log_msg_set_value(msg, LM_V_MESSAGE, pattern, strlen(pattern));
  log_msg_set_value(msg, LM_V_HOST, MYHOST, strlen(MYHOST));

  result = log_pattern_database_lookup(patterndb, msg);
  if (result)
    {
       if (result->values)
         {
           while (i < result->values->len && !g_str_equal(((LogTemplate *)g_ptr_array_index(result->values, i))->name, name))
             i++;

           if (i < result->values->len)
             {
               log_template_format(g_ptr_array_index(result->values, i), msg, 0, TS_FMT_ISO, NULL, 0, 0, val);
               found = g_str_equal(val->str, value);
             }
         }
    }

  if (!!value ^ found)
    test_fail("Value '%s' is %smatching for pattern '%s' (%d)\n", name, found ? "" : "not ", pattern, !!result);

  log_msg_unref(msg);
  g_string_free(val, TRUE);
}

void
test_rule_tag(LogPatternDatabase *patterndb, const gchar *pattern, const gchar *tag, gboolean set)
{
  LogDBResult *result;
  LogMessage *msg = log_msg_new_empty();
  guint tag_id = log_tags_get_by_name(tag);
  gboolean found = FALSE;
  gint i = 0;

  log_msg_set_value(msg, LM_V_MESSAGE, pattern, strlen(pattern));

  result = log_pattern_database_lookup(patterndb, msg);
  if (result)
    {
       if (result->tags)
         {
           while (i < result->tags->len && g_array_index(result->tags, guint, i) != tag_id)
             i++;

           if (i < result->tags->len)
             found = TRUE;
         }
    }

  if (set ^ found)
    test_fail("Tag '%s' is %sset for pattern '%s' (%d)\n", tag, found ? "" : "not ", pattern, !!result);

  log_msg_unref(msg);
}

int
main(int argc, char *argv[])
{
  LogPatternDatabase patterndb;
  gchar *filename = NULL;

  app_startup();

  if (argc > 1)
    verbose = TRUE;

  msg_init(TRUE);

  memset(&patterndb, 0x0, sizeof(LogPatternDatabase));

  g_file_open_tmp("patterndbXXXXXX.xml", &filename, NULL);
  g_file_set_contents(filename, pdb, strlen(pdb), NULL);

  if (log_pattern_database_load(&patterndb, filename))
    {
      if (!g_str_equal(patterndb.version, "2"))
        test_fail("Invalid version '%s'\n", patterndb.version);
      if (!g_str_equal(patterndb.pub_date, "2009-07-28"))
        test_fail("Invalid pub_date '%s'\n", patterndb.pub_date);

      test_rule_tag(&patterndb, "pattern11", "tag11-1", TRUE);
      test_rule_tag(&patterndb, "pattern11", "tag11-2", TRUE);
      test_rule_tag(&patterndb, "pattern11", "tag11-3", FALSE);
      test_rule_tag(&patterndb, "pattern11a", "tag11-1", TRUE);
      test_rule_tag(&patterndb, "pattern11a", "tag11-2", TRUE);
      test_rule_tag(&patterndb, "pattern11a", "tag11-3", FALSE);
      test_rule_tag(&patterndb, "pattern12", "tag12-1", FALSE);
      test_rule_tag(&patterndb, "pattern12", "tag12-2", FALSE);
      test_rule_tag(&patterndb, "pattern12", "tag12-3", FALSE);
      test_rule_tag(&patterndb, "pattern12a", "tag12-1", FALSE);
      test_rule_tag(&patterndb, "pattern12a", "tag12-2", FALSE);
      test_rule_tag(&patterndb, "pattern12a", "tag12-3", FALSE);
      test_rule_tag(&patterndb, "pattern1x", "tag1x-1", FALSE);
      test_rule_tag(&patterndb, "pattern1x", "tag1x-2", FALSE);
      test_rule_tag(&patterndb, "pattern1x", "tag1x-3", FALSE);
      test_rule_tag(&patterndb, "pattern1xa", "tag1x-1", FALSE);
      test_rule_tag(&patterndb, "pattern1xa", "tag1x-2", FALSE);
      test_rule_tag(&patterndb, "pattern1xa", "tag1x-3", FALSE);

      test_rule_value(&patterndb, "pattern11", "n11-1", "v11-1");
      test_rule_value(&patterndb, "pattern11", "n11-2", "v11-2");
      test_rule_value(&patterndb, "pattern11", "n11-3", NULL);
      test_rule_value(&patterndb, "pattern11a", "n11-1", "v11-1");
      test_rule_value(&patterndb, "pattern11a", "n11-2", "v11-2");
      test_rule_value(&patterndb, "pattern11a", "n11-3", NULL);
      test_rule_value(&patterndb, "pattern12", "n12-1", NULL);
      test_rule_value(&patterndb, "pattern12", "n12-2", NULL);
      test_rule_value(&patterndb, "pattern12", "n12-3", NULL);
      test_rule_value(&patterndb, "pattern1x", "n1x-1", NULL);
      test_rule_value(&patterndb, "pattern1x", "n1x-2", NULL);
      test_rule_value(&patterndb, "pattern1x", "n1x-3", NULL);

      test_rule_value(&patterndb, "pattern11", "vvv", MYHOST);

      log_pattern_database_free(&patterndb);
    }
  else
    fail = TRUE;

  g_unlink(filename);
  g_free(filename);
  app_shutdown();
  return  (fail ? 1 : 0);
}

