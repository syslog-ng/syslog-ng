#include "syslog-ng.h"
#include "logmsg.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#define TEST_ASSERT(x, format, value, expected)		\
  do 				\
    { 				\
        if (!(x))		\
          {			\
            printf("Testcase failed; msg='%s', cond='%s', value=" format ", expected=" format "\n", msg, __STRING(x), value, expected); \
            exit(1);		\
          }			\
    }				\
  while (0)

void 
msg_event(gint prio, const char *desc, void *tag1, ...)
{
  ;
}
  
unsigned long 
absolute_value(signed long diff)
{
  if (diff < 0)
    return -diff;
  else
    return diff;
}

int 
testcase(gchar *msg, 
         gint parse_flags,
         gint expected_pri, 
         unsigned long expected_stamp_sec, 
         unsigned long expected_stamp_usec, 
         unsigned long expected_stamp_ofs,
         const gchar *expected_date, 
         const gchar *expected_host, 
         const gchar *expected_program, 
         const gchar *expected_msg)
{
  LogMessage *logmsg;
  time_t now;
  
  logmsg = log_msg_new(msg, strlen(msg), NULL, parse_flags);
  TEST_ASSERT(logmsg->pri == expected_pri, "%d", logmsg->pri, expected_pri);
  if (expected_stamp_sec)
    {
      TEST_ASSERT(logmsg->stamp.time.tv_sec == expected_stamp_sec, "%d", (int) logmsg->stamp.time.tv_sec, (int) expected_stamp_sec);
      TEST_ASSERT(logmsg->stamp.time.tv_usec == expected_stamp_usec, "%d", (int) logmsg->stamp.time.tv_usec, (int) expected_stamp_usec);
      TEST_ASSERT(logmsg->stamp.zone_offset == expected_stamp_ofs, "%d", (int) logmsg->stamp.zone_offset, (int) expected_stamp_ofs);
    }
  else
    {
      time(&now);
      TEST_ASSERT(absolute_value(logmsg->stamp.time.tv_sec - now) < 1, "%d", 0, 0);
    }
  if (expected_date)
    {
      TEST_ASSERT(strcmp(logmsg->date->str, expected_date) == 0, "%s", logmsg->date->str, expected_date);
    }
  TEST_ASSERT(strcmp(logmsg->host->str, expected_host) == 0, "%s", logmsg->host->str, expected_host);
  TEST_ASSERT(strcmp(logmsg->program->str, expected_program) == 0, "%s", logmsg->program->str, expected_program);
  TEST_ASSERT(strcmp(logmsg->msg->str, expected_msg) == 0, "%s", logmsg->msg->str, expected_msg);
  log_msg_unref(logmsg);
  return 0;
}

int 
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  putenv("TZ=UTC");
  timezone = 0;
  testcase("<7>Sep  7 10:43:21 bzorp openvpn[2499]: PTHREAD support initialized", 0, 
           7, 			// pri
           1094553801, 0, 0,	// timestamp (sec/usec/zone)
           "Sep  7 10:43:21",   // originally formatted timestamp
           "bzorp",		// host
           "openvpn",		// openvpn
           "openvpn[2499]: PTHREAD support initialized" // msg
           );

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, 
           15, 			// pri
           0, 0, 0,		// timestamp (sec/usec/zone)
           NULL,                // originally formatted timestamp
           "",		// host
           "openvpn",		// openvpn
           "openvpn[2499]: PTHREAD support initialized" // msg
           );

  testcase("<7>2004-09-07T10:43:21.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, 
           7, 			// pri
           1094550201, 156000, -3600,	// timestamp (sec/usec/zone)
           "2004-09-07T10:43:21.156+01:00",   // originally formatted timestamp
           "bzorp",		// host
           "openvpn",		// openvpn
           "openvpn[2499]: PTHREAD support initialized" // msg
           );
//  testcase("<30>Oct 14 17:25:34 isdnlog: \nOct 14 17:25:34 tei 69 calling ? () with T-Easy 520  HANGUP\r", 0);
  return 0;
}


