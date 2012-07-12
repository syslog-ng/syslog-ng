
#include "apphook.h"
#include "tags.h"
#include "logmsg.h"
#include "messages.h"
#include "filter.h"
#include "dbparser.h"
#include "patterndb-int.h"
#include "cfg.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>

gboolean fail = FALSE;
gboolean verbose = FALSE;
GlobalConfig *configuration;

gchar *pdb_part_1 ="<?xml version='1.0' encoding='UTF-8'?>\
          <patterndb version='3' pub_date='2011-05-25'>\
            <ruleset name='testruleset' id='480de478-d4a6-4a7f-bea4-0c0245d361e1'>\
            <pattern>";
gchar *pdb_part_2 = "</pattern>\
                    <rule id='1' class='test1' provider='my'>\
                        <patterns>\
                         <pattern>";
gchar *pdb_part_3 =  "</pattern>\
                      </patterns>\
                    </rule>\
            </ruleset>\
        </patterndb>";


/********** testing a simple match *************/
gchar * test1 [] = {
"program1", // program pattern
"rule1", // rule pattern
"program1", // program of the message to send in
"rule1 foo bar", // message to send in
// expected name-value pairs, terminated by null
NULL
};

/********** testing a rule match *************/
gchar * test2 [] = {
"program1", // program pattern
"rule@NUMBER:test_number@", // rule pattern
"program1", // program of the message to send in
"rule1", // message to send in
// expected name-value pairs, terminated by null
"test_number", "1",
NULL
};

/********** testing a match in the program *************/
gchar * test3 [] = {
"program@NUMBER:test_program_number@", // program pattern
"rule", // rule pattern
"program1", // program of the message to send in
"rule", // message to send in
// expected name-value pairs, terminated by null
"test_program_number", "1",
NULL
};

/********** testing a match in both the program and the message *************/
gchar * test4 [] = {
"program@NUMBER:test_program_number@", // program pattern
"rule@NUMBER:test_rule_number@", // rule pattern
"program123", // program of the message to send in
"rule456", // message to send in
// expected name-value pairs, terminated by null
"test_program_number", "123",
"test_rule_number", "456",
NULL
};

/********** testing multiple matches in the message part *************/
gchar * test5 [] = {
"program", // program pattern
"@IPv4:ip_1@ @IPv4:ip_2@", // rule pattern
"program", // program of the message to send in
"1.2.3.4 5.5.5.5", // message to send in
// expected name-value pairs, terminated by null
"ip_1", "1.2.3.4",
"ip_2", "5.5.5.5",
NULL
};

/********** testing multiple matches in the program part *************/
gchar * test6 [] = {
"program_a@ESTRING:test_program_number_a:|@ program_b@ESTRING:test_program_number_b:|@", // program pattern
"rule", // rule pattern
"program_a123| program_b456|", // program of the message to send in
"rule", // message to send in
// expected name-value pairs, terminated by null
"test_program_number_a", "123",
"test_program_number_b", "456",
NULL
};

/********** testing multiple matches in both the program and the message part *************/
gchar * test7 [] = {
"program_a@ESTRING:test_program_number_a:|@ program_b@ESTRING:test_program_number_b:|@", // program pattern
"@IPv4:ip_1@ @IPv4:ip_2@", // rule pattern
"program_a123| program_b456|", // program of the message to send in
"1.2.3.4 5.5.5.5", // message to send in
// expected name-value pairs, terminated by null
"ip_1", "1.2.3.4",
"ip_2", "5.5.5.5",
"test_program_number_a", "123",
"test_program_number_b", "456",
NULL
};

/********** testing a match in both the program and the message, with a collision *************/
gchar * test8 [] = {
"program@NUMBER:test_program_number@", // program pattern
"rule@NUMBER:test_program_number@", // rule pattern
"program123", // program of the message to send in
"rule456", // message to send in
// expected name-value pairs, terminated by null
"test_program_number", "456", // the latter one wins.
NULL
};

/********** testing a rule match with a dot in the name *************/
gchar * test9 [] = {
"program1", // program pattern
"rule@NUMBER:testprefix.test_number@", // rule pattern
"program1", // program of the message to send in
"rule1", // message to send in
// expected name-value pairs, terminated by null
"testprefix.test_number", "1",
NULL
};

/********** testing a match in the program with a dot in the name*************/
gchar * test10 [] = {
"program@NUMBER:testprefix.test_program_number@", // program pattern
"rule", // rule pattern
"program1", // program of the message to send in
"rule", // message to send in
// expected name-value pairs, terminated by null
"testprefix.test_program_number", "1",
NULL
};

/********** testing a rule match with multiple dots in the name *************/
gchar * test11 [] = {
"program1", // program pattern
"rule@NUMBER:.testprefix.test_number@", // rule pattern
"program1", // program of the message to send in
"rule1", // message to send in
// expected name-value pairs, terminated by null
".testprefix.test_number", "1",
NULL
};

/********** testing a match in the program with multiple dots in the name*************/
gchar * test12 [] = {
"program@NUMBER:.testprefix.test_program_number@", // program pattern
"rule", // rule pattern
"program1", // program of the message to send in
"rule", // message to send in
// expected name-value pairs, terminated by null
".testprefix.test_program_number", "1",
NULL
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
create_pattern_db(PDBRuleSet *patterndb, gchar *program_pattern, gchar *rule_pattern, gchar ** filename)
{
  GString *str = g_string_new(pdb_part_1);
  GError *err = NULL;
  int retval;
  memset(patterndb, 0x0, sizeof(PDBRuleSet));

  g_string_append(str,program_pattern);
  g_string_append(str,pdb_part_2);
  g_string_append(str,rule_pattern);
  g_string_append(str,pdb_part_3);

#ifndef _WIN32
  g_file_open_tmp("patterndbXXXXXX.xml", filename, &err);
#else
  *filename = g_strdup("patterndbXXXXXX.xml");
  retval = g_mkstemp(*filename);
  if (retval != -1)
         close(retval);
#endif
  if (!g_file_set_contents(*filename, str->str, str->len, &err))
  {
       fprintf(stderr,"ERROR OCCURED!\n");
  }

  pdb_rule_set_load(patterndb, configuration, *filename, NULL);

  g_string_free(str,TRUE);
}

void
clean_pattern_db(PDBRuleSet *patterndb, gchar**filename)
{
  pdb_rule_set_free(patterndb);

  g_unlink(*filename);
  g_free(*filename);

}

void
test_case(gchar **testcase)
{
  PDBRuleSet *patterndb = g_new0(PDBRuleSet,1);
  gchar *filename = NULL;
  gchar *name, *value;
  PDBRule *result;
  LogMessage *msg = log_msg_new_empty();
  GString *val = g_string_sized_new(256);
  gint index = 4;

  create_pattern_db(patterndb, testcase[0], testcase[1], &filename);

  log_msg_set_value(msg, LM_V_PROGRAM, testcase[2], strlen(testcase[2]));
  log_msg_set_value(msg, LM_V_MESSAGE, testcase[3], strlen(testcase[3]));
  log_msg_set_value(msg, LM_V_HOST, MYHOST, strlen(MYHOST));

  /* NOTE: we only use result to check if there were any -- the parsed name-value pairs are
   * added directly to the logmsg itself. It is a bit confusing behavior, because values that
   * are manually hardcoded into the patterndb are returned in the PDBRule
   * log_pattern_database_lookup(), while parsed entries are only added to the LogMessage.
   * This unit test tests the latter, the entries set in the LogMessage.
   */
  result = pdb_rule_set_lookup(patterndb, msg, NULL);
  if (result)
    {
       /* we only need to search for values if the list in the testcase is not empty */
      if (testcase[index] != NULL)
        {
          while(testcase[index] != NULL)
            {
              name = testcase[index++];
              value = testcase[index++];
              gssize value_len;

              const gchar *result = log_msg_get_value(msg, log_msg_get_value_handle(name), &value_len);
              if (!result || value_len == 0 || strncmp(result, value, value_len) != 0)
                {
                  /* NOTE: we only look for the first failure */
                  test_fail("ERROR:\nName-value pair '%s' - '%s' is not found\npatterns: program='%s', rule='%s'\nmessage: program='%s' msg='%s'\nfound='%s'\n------\n",
                    name, value, testcase[0], testcase[1], testcase[2], testcase[3], result ? result : "");
                }
            }
        }
    }
  else
    test_fail("ERROR:\nPattern lookup yielded no results at all\npatterns: program='%s', rule='%s'\nmessage: program='%s' msg='%s'\n------\n",
      testcase[0], testcase[1], testcase[2], testcase[3]);

  log_msg_unref(msg);
  g_string_free(val, TRUE);

  clean_pattern_db(patterndb, &filename);
}

int
main(int argc, char *argv[])
{
  configuration = cfg_new(0x0303);
  app_startup();

  //if (argc > 1)
    verbose = TRUE;

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
  test_case(test12);

  app_shutdown();
  return  (fail ? 1 : 0);
}
