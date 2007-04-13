#include "syslog-ng.h"
#include "syslog-names.h"
#include "filter.h"
#include "logmsg.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int debug = 1;

static gint
facility_bits(gchar *fac)
{
  return 1 << syslog_lookup_facility(fac);
}

static gint
level_bits(gchar *lev)
{
  return 1 << syslog_lookup_level(lev);
}

void
testcase(gchar *msg, 
         gint parse_flags,
         FilterExprNode *f,
         gboolean expected_result)
{
  LogMessage *logmsg;
  gboolean res;
  static gint testno = 0;
  
  testno++;
  logmsg = log_msg_new(msg, strlen(msg), NULL, parse_flags);
  
  res = filter_expr_eval(f, logmsg);
  if (res != expected_result)
    {
      fprintf(stderr, "Filter test failed; num='%d', msg='%s'\n", testno, msg);
      exit(1);
    }
    
  f->comp = 1;
  res = filter_expr_eval(f, logmsg);
  if (res != !expected_result)
    {
      fprintf(stderr, "Filter test failed (negated); num='%d', msg='%s'\n", testno, msg);
      exit(1);
    }
  
  log_msg_unref(logmsg);
  filter_expr_free(f);
}

#define TEST_ASSERT(cond)                                       \
  if (!(cond))                                                  \
    {                                                           \
      fprintf(stderr, "Test assertion failed at %d\n", __LINE__);    \
      exit(1);                                                  \
    }

int 
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  gint i;
  
  msg_init(1);
  
  
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("user")), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("daemon")), 0);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("daemon") | facility_bits("user")), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("uucp") | facility_bits("local4")), 0);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | LOG_USER), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | LOG_DAEMON), 0);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("debug") | level_bits("emerg")), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("emerg")), 0);
  
  /* 8 tests so far */
  for (i = 0; i < 10; i++)
    {
      testcase("<0> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("emerg")), 1);
      testcase("<1> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("alert")), 1);
      testcase("<2> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("crit")), 1);
      testcase("<3> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("err")), 1);
      testcase("<4> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("warning")), 1);
      testcase("<5> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("notice")), 1);
      testcase("<6> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("info")), 1);
      testcase("<7> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("debug")), 1);
    }
    
  /* 88 tests so far */
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_prog_new("^openvpn$"), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_prog_new("^open$"), 0);
  fprintf(stderr, "One \"invalid regular expression\" message is to be expected\n");
  TEST_ASSERT(filter_prog_new("((") == NULL);

  /* 90 tests so far */
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_host_new("^host$"), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_host_new("^hos$"), 0);
  fprintf(stderr, "One \"invalid regular expressions\" message is to be expected\n");
  TEST_ASSERT(filter_host_new("((") == NULL);

  /* 92 tests so far */
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_match_new(" PTHREAD "), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_match_new("^PTHREAD$"), 0);
  fprintf(stderr, "One \"invalid regular expression\" message is to be expected\n");
  TEST_ASSERT(filter_match_new("((") == NULL);

  /* 94 tests so far */
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(filter_match_new(" PTHREAD "), filter_match_new("PTHREAD")), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(filter_match_new(" PTHREAD "), filter_match_new("^PTHREAD$")), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(filter_match_new("^PTHREAD$"), filter_match_new(" PTHREAD ")), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(filter_match_new(" PAD "), filter_match_new("^PTHREAD$")), 0);

  /* 98 tests so far */
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(filter_match_new(" PTHREAD "), filter_match_new("PTHREAD")), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(filter_match_new(" PTHREAD "), filter_match_new("^PTHREAD$")), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(filter_match_new("^PTHREAD$"), filter_match_new(" PTHREAD ")), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(filter_match_new(" PAD "), filter_match_new("^PTHREAD$")), 0);

  return 0;
}


