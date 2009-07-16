#include "syslog-ng.h"
#include "logmsg.h"
#include "serialize.h"
#include "apphook.h"
#include "gsockaddr.h"
#include "logpipe.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TEST_ASSERT(x, format, value, expected)		\
  do 				\
    { 				\
        if (!(x))		\
          {			\
            fprintf(stderr, "Testcase failed; msg='%s', cond='%s', value=" format ", expected=" format "\n", msg, #x, value, expected); \
            exit(1);		\
          }			\
    }				\
  while (0)


unsigned long
absolute_value(signed long diff)
{
  if (diff < 0)
    return -diff;
  else
    return diff;
}
//FIXME use log msg value lookup
void
check_val_in_hash(gchar *msg, LogMessage *self, const gchar* pair[2])
{
   TEST_ASSERT(strcmp(g_hash_table_lookup(self->values, pair[0]), pair[1]) == 0, "%s", (gchar*)g_hash_table_lookup(self->values, pair[0]), pair[1]);
}

void
check_val_in_hash_clone(gchar *msg, LogMessage *self,LogMessage *msg_clone , const gchar* pair[2])
{
  const  gchar * a1 = g_hash_table_lookup(self->values, pair[0]);
  const  gchar * a2 = g_hash_table_lookup(msg_clone->values, pair[0]);
  TEST_ASSERT(strcmp( a1, pair[1]) == 0 && strcmp(a2, pair[1] ) == 0, "%s", a1, a2);
}


int
testcase(gchar *msg,
         gint parse_flags, /* LP_NEW_PROTOCOL */
         gchar *bad_hostname_re,
         gint expected_pri,
         guint expected_version,
         unsigned long expected_stamps_sec,
         unsigned long expected_stamps_usec,
         unsigned long expected_stamps_ofs,
         const gchar *expected_host,
         const gchar *expected_msg,
         const gchar *expected_program,
         const gchar *expected_sd_str,
         const gchar *expected_process_id,
         const gchar *expected_message_id
         )
{
  LogMessage *logmsg, *cloned;
  time_t now;
  regex_t bad_hostname;
  GSockAddr *addr = g_sockaddr_inet_new("10.10.10.10", 1010);
  gchar logmsg_addr[256], cloned_addr[256];
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  GString *sd_str = g_string_sized_new(0);

  if (bad_hostname_re)
    TEST_ASSERT(regcomp(&bad_hostname, bad_hostname_re, REG_NOSUB | REG_EXTENDED) == 0, "%d", 0, 0);

  logmsg = log_msg_new(msg, strlen(msg), addr, parse_flags, bad_hostname_re ? &bad_hostname : NULL, -1, 0xFFFF);
  TEST_ASSERT(logmsg->pri == expected_pri, "%d", logmsg->pri, expected_pri);
  if (expected_stamps_sec)
    {
      if (expected_stamps_sec != 1)
        {
            TEST_ASSERT(logmsg->timestamps[LM_TS_STAMP].time.tv_sec == expected_stamps_sec, "%d", (int) logmsg->timestamps[LM_TS_STAMP].time.tv_sec, (int) expected_stamps_sec);
          }
        TEST_ASSERT(logmsg->timestamps[LM_TS_STAMP].time.tv_usec == expected_stamps_usec, "%d", (int) logmsg->timestamps[LM_TS_STAMP].time.tv_usec, (int) expected_stamps_usec);
        TEST_ASSERT(logmsg->timestamps[LM_TS_STAMP].zone_offset == expected_stamps_ofs, "%d", (int) logmsg->timestamps[LM_TS_STAMP].zone_offset, (int) expected_stamps_ofs);
    }
  else
    {
      time(&now);
      TEST_ASSERT(absolute_value(logmsg->timestamps[LM_TS_STAMP].time.tv_sec - now) < 1, "%d", 0, 0);
    }
  TEST_ASSERT(strcmp(logmsg->host, expected_host) == 0, "%s", logmsg->host, expected_host);
  TEST_ASSERT(strcmp(logmsg->program, expected_program) == 0, "%s", logmsg->program, expected_program);
  TEST_ASSERT(strcmp(logmsg->message, expected_msg) == 0, "%s", logmsg->message, expected_msg);
  TEST_ASSERT(strcmp(logmsg->pid, expected_process_id) == 0, "%s", logmsg->pid, expected_process_id);
  TEST_ASSERT(strcmp(logmsg->msgid, expected_message_id) == 0, "%s", logmsg->msgid, expected_message_id);

  /* SD elements */
  log_msg_format_sdata(logmsg, sd_str);
  TEST_ASSERT(strcmp(sd_str->str, expected_sd_str) == 0, "%s", sd_str->str, expected_sd_str);

  /* check if the sockaddr matches */
  g_sockaddr_format(logmsg->saddr, logmsg_addr, sizeof(logmsg_addr), GSA_FULL);

  path_options.flow_control = FALSE;
  cloned = log_msg_clone_cow(logmsg, &path_options);

  g_sockaddr_format(cloned->saddr, cloned_addr, sizeof(cloned_addr), GSA_FULL);
  TEST_ASSERT(strcmp(logmsg_addr, cloned_addr) == 0, "%s", cloned_addr, logmsg_addr);

  TEST_ASSERT(logmsg->pri == cloned->pri, "%d", logmsg->pri, cloned->pri);
  TEST_ASSERT(logmsg->timestamps[LM_TS_STAMP].time.tv_sec == cloned->timestamps[LM_TS_STAMP].time.tv_sec, "%d", (int) logmsg->timestamps[LM_TS_STAMP].time.tv_sec, (int) cloned->timestamps[LM_TS_STAMP].time.tv_sec);
  TEST_ASSERT(logmsg->timestamps[LM_TS_STAMP].time.tv_usec == cloned->timestamps[LM_TS_STAMP].time.tv_usec, "%d", (int) logmsg->timestamps[LM_TS_STAMP].time.tv_usec, (int) cloned->timestamps[LM_TS_STAMP].time.tv_usec);
  TEST_ASSERT(logmsg->timestamps[LM_TS_STAMP].zone_offset == cloned->timestamps[LM_TS_STAMP].zone_offset, "%d", (int) logmsg->timestamps[LM_TS_STAMP].zone_offset, (int) cloned->timestamps[LM_TS_STAMP].zone_offset);
  TEST_ASSERT(strcmp(logmsg->host, cloned->host) == 0, "%s", logmsg->host, cloned->host);
  TEST_ASSERT(strcmp(logmsg->program, cloned->program) == 0, "%s", logmsg->program, cloned->program);
  TEST_ASSERT(strcmp(logmsg->message, cloned->message) == 0, "%s", logmsg->message, cloned->message);
  TEST_ASSERT(strcmp(logmsg->pid, cloned->pid) == 0, "%s", logmsg->pid, cloned->pid);
  TEST_ASSERT(strcmp(logmsg->msgid, cloned->msgid) == 0, "%s", logmsg->msgid, cloned->msgid);

  /* SD elements */
  log_msg_format_sdata(cloned, sd_str);
  TEST_ASSERT(strcmp(sd_str->str, expected_sd_str) == 0, "%s", sd_str->str, expected_sd_str);


  log_msg_set_host(cloned, g_strdup("newhost"), -1);
  log_msg_set_host_from(cloned, g_strdup("newhost"), -1);
  log_msg_set_message(cloned, g_strdup("newmsg"), -1);
  log_msg_set_program(cloned, g_strdup("newprogram"), -1);
  log_msg_set_pid(cloned, g_strdup("newpid"), -1);
  log_msg_set_msgid(cloned, g_strdup("newmsgid"), -1);
  log_msg_set_source(cloned, g_strdup("newsource"), -1);
  log_msg_add_dyn_value(cloned, "newvalue", "newvalue");

  /* retest values in original logmsg */

  TEST_ASSERT(strcmp(logmsg->host, expected_host) == 0, "%s", logmsg->host, expected_host);
  TEST_ASSERT(strcmp(logmsg->program, expected_program) == 0, "%s", logmsg->program, expected_program);
  TEST_ASSERT(strcmp(logmsg->message, expected_msg) == 0, "%s", logmsg->message, expected_msg);
  TEST_ASSERT(strcmp(logmsg->pid, expected_process_id) == 0, "%s", logmsg->pid, expected_process_id);
  TEST_ASSERT(strcmp(logmsg->msgid, expected_message_id) == 0, "%s", logmsg->msgid, expected_message_id);
  TEST_ASSERT(strcmp(logmsg->source, "") == 0, "%s", logmsg->source, "");

  /* check newly set values in cloned */

  TEST_ASSERT(strcmp(cloned->host, "newhost") == 0, "%s", cloned->host, "newhost");
  TEST_ASSERT(strcmp(cloned->host_from, "newhost") == 0, "%s", cloned->host_from, "newhost");
  TEST_ASSERT(strcmp(cloned->program, "newprogram") == 0, "%s", cloned->program, "newprogram");
  TEST_ASSERT(strcmp(cloned->message, "newmsg") == 0, "%s", cloned->message, "newmsg");
  TEST_ASSERT(strcmp(cloned->pid, "newpid") == 0, "%s", cloned->pid, "newpid");
  TEST_ASSERT(strcmp(cloned->msgid, "newmsgid") == 0, "%s", cloned->msgid, "newmsgid");
  TEST_ASSERT(strcmp(cloned->source, "newsource") == 0, "%s", cloned->msgid, "newsource");

  log_msg_unref(cloned);
  log_msg_unref(logmsg);
  g_string_free(sd_str, TRUE);
  return 0;
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();

  putenv("TZ=MET-1METDST");
  tzset();

  testcase("<7>1 2006-10-29T01:59:59.156+01:00 mymachine.example.com evntslog - ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...",
           LP_SYSLOG_PROTOCOL, //flags
           NULL,  //bad host
           7, 			// pri
           1,  //version
           1162083599, 156000, 3600,	// timetimestamps[LM_TS_STAMP] (sec/usec/zone)
           "mymachine.example.com",		// host
           "BOMAn application event log entry...", // msg
           "evntslog", //app
           "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"]", //sd_str
           "",//processid
           "ID47"
           );

  /*NOTE bad sd format FIXME: check BOM*/
  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [exampleSDID@0 iut=\"3\"] [eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...",
           LP_SYSLOG_PROTOCOL, //flags
           NULL,  //bad host
           132, 			// pri
           12,  //version
           1162083599, 156000, 3600,	// timetimestamps[LM_TS_STAMP] (sec/usec/zone)
           "mymachine",		// host
           "[eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...", // msg
           "evntslog", //app
           "[exampleSDID@0 iut=\"3\"]", //sd_str
           "",//processid
           ""//msgid
           );

  app_shutdown();
  return 0;
}

