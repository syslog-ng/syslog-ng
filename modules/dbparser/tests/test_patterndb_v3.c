
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

gchar *pdb_prefix ="<?xml version='1.0' encoding='UTF-8'?>\
          <patterndb version='3' pub_date='2010-02-22'>\
            <ruleset name='test1_program' id='480de478-d4a6-4a7f-bea4-0c0245d361e1'>\
                <pattern>test</pattern>\
                    <rule id='1' class='test1' provider='my'>\
                        <patterns>\
                         <pattern>";

gchar *pdb_postfix =  "</pattern>\
                      </patterns>\
                    </rule>\
            </ruleset>\
        </patterndb>";


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

gchar * test7 [] = {
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

gchar * test8 [] = {
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

gchar * test9 [] = {
"@NUMBER:TEST@",
"1234",
"1.2",
"1.2.3.4",
"1234ab",
NULL,
"ab1234",
"1,2",NULL
};

gchar * test10 [] = {
"@QSTRING:TEST:&lt;&gt;@",
"<aa bb>",
"< aabb >",
NULL,
"aabb>",
"<aabb",NULL
};

gchar * test11 [] = {
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
test_rule_pattern(PatternDB *patterndb, const gchar *pattern, const gchar *rule, gboolean match)
{
  gboolean result;
  LogMessage *msg = log_msg_new_empty();
  static LogTemplate *templ;

  GString *res = g_string_sized_new(128);
  static TimeZoneInfo *tzinfo = NULL;

  if (!tzinfo)
    tzinfo = time_zone_info_new(NULL);
  if (!templ)
    templ = log_template_new(configuration, "dummy", "$TEST");

  log_msg_set_value(msg, LM_V_HOST, MYHOST, strlen(MYHOST));
  log_msg_set_value(msg, LM_V_PROGRAM, "test", strlen(MYHOST));
  log_msg_set_value(msg, LM_V_MESSAGE, pattern, strlen(pattern));

  result = pattern_db_lookup(patterndb, msg, NULL);

  log_template_format(templ, msg, NULL, LTZ_LOCAL, 0, res);

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
create_pattern_db(PatternDB **patterndb, gchar *pattern, gchar ** filename)
{
  GString *str = g_string_new(pdb_prefix);

  *patterndb = pattern_db_new();
  g_string_append(str,pattern);
  g_string_append(str,pdb_postfix);

  g_file_open_tmp("patterndbXXXXXX.xml", filename, NULL);
  g_file_set_contents(*filename, str->str, str->len, NULL);

  if (pattern_db_load(*patterndb, configuration, *filename, NULL))
    {
      if (!g_str_equal((*patterndb)->version, "3"))
        test_fail("Invalid version '%s'\n", (*patterndb)->version);
      if (!g_str_equal((*patterndb)->pub_date, "2010-02-22"))
        test_fail("Invalid pub_date '%s'\n", (*patterndb)->pub_date);
    }
  g_string_free(str,TRUE);
}

void
clean_pattern_db(PatternDB **patterndb, gchar**filename)
{
  pattern_db_free(*patterndb);
  *patterndb = NULL;

  g_unlink(*filename);
  g_free(*filename);

}

void
test_case(gchar** test)
{
  PatternDB *patterndb;
  gchar *filename = NULL;
  gint index = 1;

  create_pattern_db(&patterndb, test[0], &filename);

  while(test[index] != NULL)
    test_rule_pattern(patterndb, test[index++], test[0], TRUE);
  while(test[index] != NULL)
    test_rule_pattern(patterndb, test[index++], test[0], FALSE);

  clean_pattern_db(&patterndb, &filename);
}

int
main(int argc, char *argv[])
{
  app_startup();

  if (argc > 1)
    verbose = TRUE;

  configuration = cfg_new(0x0302);
  pattern_db_global_init();

  msg_init(TRUE);
  test_case(test1);
  test_case(test2);
  test_case(test3);
  test_case(test4);
  test_case(test5);
  test_case(test6);
  test_case(test7);
  test_case(test8);
  test_case(test9);
  test_case(test10);
  test_case(test11);

  app_shutdown();
  return  (fail ? 1 : 0);
}
