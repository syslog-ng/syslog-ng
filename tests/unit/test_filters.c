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
  return 1 << syslog_name_lookup_facility_by_name(fac);
}

static gint
level_bits(gchar *lev)
{
  return 1 << syslog_name_lookup_level_by_name(lev);
}

static gint
level_range(gchar *from, gchar *to)
{
  int r1, r2;
  
  r1 = syslog_name_lookup_level_by_name(from); 
  r2 = syslog_name_lookup_level_by_name(to);
  return syslog_make_range(r1, r2);
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
  logmsg = log_msg_new(msg, strlen(msg), NULL, parse_flags, NULL);
  logmsg->saddr = g_sockaddr_inet_new("10.10.0.1", 5000);
  
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
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | (LOG_USER >> 3)), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | (LOG_DAEMON >> 3)), 0);
  
  testcase("<2> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("kern")), 1);
  testcase("<2> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | (LOG_KERN >> 3)), 1);
  
  testcase("<128> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("local0")), 1);
  testcase("<128> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | (LOG_LOCAL0 >> 3)), 1);
  testcase("<32> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("local1")), 0);
  testcase("<32> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("auth")), 1);
  testcase("<32> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | (LOG_AUTH >> 3)), 1);
  testcase("<80> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("authpriv")), 1);
  testcase("<80> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | (LOG_AUTHPRIV >> 3)), 1);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("debug") | level_bits("emerg")), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("emerg")), 0);

  testcase("<8> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 1);
  testcase("<9> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 1);
  testcase("<10> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 1);
  testcase("<11> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 0);
  testcase("<12> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 0);
  testcase("<13> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 0);
  testcase("<14> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 0);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 0);

  testcase("<8> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 0);
  testcase("<9> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 0);
  testcase("<10> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 0);
  testcase("<11> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 0);
  testcase("<12> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 0);
  testcase("<13> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 1);
  testcase("<14> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 1);

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
    
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_prog_new("^openvpn$"), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_prog_new("^open$"), 0);
  fprintf(stderr, "One \"invalid regular expression\" message is to be expected\n");
  TEST_ASSERT(filter_prog_new("((") == NULL);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_host_new("^host$"), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_host_new("^hos$"), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_host_new("pthread"), 0);
  fprintf(stderr, "One \"invalid regular expressions\" message is to be expected\n");
  TEST_ASSERT(filter_host_new("((") == NULL);

  fprintf(stderr, "One \"invalid regular expressions\" message is to be expected\n");
  TEST_ASSERT(filter_host_new("(?iana") == NULL);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_match_new(" PTHREAD "), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_match_new("^PTHREAD$"), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_match_new("(?i)pthread"), 1);
  fprintf(stderr, "One \"invalid regular expression\" message is to be expected\n");
  TEST_ASSERT(filter_match_new("((") == NULL);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_netmask_new("10.10.0.0/16"), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_netmask_new("10.10.0.0/24"), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_netmask_new("10.10.10.0/24"), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_netmask_new("0.0.10.10/24"), 0);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(filter_match_new(" PTHREAD "), filter_match_new("PTHREAD")), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(filter_match_new(" PTHREAD "), filter_match_new("^PTHREAD$")), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(filter_match_new("^PTHREAD$"), filter_match_new(" PTHREAD ")), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(filter_match_new(" PAD "), filter_match_new("^PTHREAD$")), 0);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(filter_match_new(" PTHREAD "), filter_match_new("PTHREAD")), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(filter_match_new(" PTHREAD "), filter_match_new("^PTHREAD$")), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(filter_match_new("^PTHREAD$"), filter_match_new(" PTHREAD ")), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(filter_match_new(" PAD "), filter_match_new("^PTHREAD$")), 0);

  return 0;
}


