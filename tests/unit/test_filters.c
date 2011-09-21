#include "syslog-ng.h"
#include "syslog-names.h"
#include "filter.h"
#include "logmsg.h"
#include "apphook.h"
#include "plugin.h"
#include "filter-expr-grammar.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int debug = 1;
GSockAddr *sender_saddr;
MsgFormatOptions parse_options;

static gint
facility_bits(gchar *fac)
{
  return 1 << (syslog_name_lookup_facility_by_name(fac) >> 3);
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

FilterExprNode *
create_posix_regexp_filter(NVHandle value, gchar* regexp, gint flags)
{
  FilterRE *f;
  f = (FilterRE*)filter_re_new(value);
  filter_re_set_matcher(f, log_matcher_posix_re_new());
  filter_re_set_flags(f, flags);
  if (filter_re_set_regexp(f, regexp))
    return &f->super;
  else
    return NULL;
}

FilterExprNode *
create_posix_regexp_match(gchar* regexp, gint flags)
{
  FilterRE *f;
  f = (FilterRE*)filter_match_new();
  filter_re_set_matcher(f, log_matcher_posix_re_new());
  filter_re_set_flags(f, flags);
  if (filter_re_set_regexp(f, regexp))
    return &f->super;
  else
    return NULL;
}

LogTemplate *
create_template(const gchar *template)
{
  LogTemplate *t;

  t = log_template_new(configuration, NULL);
  log_template_compile(t, template, NULL);
  return t;
}

#if ENABLE_PCRE
FilterExprNode *
create_pcre_regexp_filter(gint field, gchar* regexp, gint flags)
{
  FilterRE *f;
  f = (FilterRE*)filter_re_new(field);
  filter_re_set_matcher(f, log_matcher_pcre_re_new());
  filter_re_set_flags(f, flags);
  if (filter_re_set_regexp(f, regexp))
    return &f->super;
  else
    return NULL;
}

FilterExprNode *
create_pcre_regexp_match(gchar* regexp, gint flags)
{
  FilterRE *f;
  f = (FilterRE*)filter_match_new();
  filter_re_set_matcher(f, log_matcher_pcre_re_new());
  filter_re_set_flags(f, flags);
  if (filter_re_set_regexp(f, regexp))
    return &f->super;
  else
    return NULL;
}
#endif

void
testcase(gchar *msg,
         FilterExprNode *f,
         gboolean expected_result)
{
  LogMessage *logmsg;
  gboolean res;
  static gint testno = 0;

  testno++;
  logmsg = log_msg_new(msg, strlen(msg), NULL, &parse_options);
  logmsg->saddr = g_sockaddr_ref(sender_saddr);

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
  filter_expr_unref(f);
}

void
testcase_with_backref_chk(gchar *msg,
         FilterExprNode *f,
         gboolean expected_result,
         const gchar *name,
         const gchar *value
         )
{
  LogMessage *logmsg;
  const gchar *value_msg;
  NVTable *nv_table;
  gboolean res;
  static gint testno = 0;
  gssize length;
  NVHandle nonasciiz = log_msg_get_value_handle("NON-ASCIIZ");
  gssize msglen;
  gchar buf[1024];

  testno++;
  logmsg = log_msg_new(msg, strlen(msg), NULL, &parse_options);
  logmsg->saddr = g_sockaddr_inet_new("10.10.0.1", 5000);

  /* NOTE: we test how our filters cope with non-zero terminated values. We don't change message_len, only the value */
  g_snprintf(buf, sizeof(buf), "%sAAAAAAAAAAAA", log_msg_get_value(logmsg, LM_V_MESSAGE, &msglen));
  log_msg_set_value(logmsg, log_msg_get_value_handle("MESSAGE2"), buf, -1);

  /* add a non-zero terminated indirect value which contains the whole message */
  log_msg_set_value_indirect(logmsg, nonasciiz, log_msg_get_value_handle("MESSAGE2"), 0, 0, msglen);

  nv_table = nv_table_ref(logmsg->payload);
  res = filter_expr_eval(f, logmsg);
  if (res != expected_result)
    {
      fprintf(stderr, "Filter test failed; num='%d', msg='%s'\n", testno, msg);
      exit(1);
    }
  nv_table_unref(nv_table);
  f->comp = 1;

  nv_table = nv_table_ref(logmsg->payload);
  res = filter_expr_eval(f, logmsg);
  if (res != !expected_result)
    {
      fprintf(stderr, "Filter test failed (negated); num='%d', msg='%s'\n", testno, msg);
      exit(1);
    }
  value_msg = log_msg_get_value(logmsg, log_msg_get_value_handle(name), &length);
  nv_table_unref(nv_table);
  if(value == NULL || value[0] == 0)
     {
       if (value_msg != NULL && value_msg[0] != 0)
         {
           fprintf(stderr, "Filter test failed (NULL value chk); num='%d', msg='%s', expected_value='%s', value_in_msg='%s'", testno, msg, value, value_msg);
           exit(1);
         }
      }
  else
    {
      if (strncmp(value_msg, value, length) != 0)
        {
          fprintf(stderr, "Filter test failed (value chk); num='%d', msg='%s', expected_value='%s', value_in_msg='%s'", testno, msg, value, value_msg);
          exit(1);
        }
    }
  log_msg_unref(logmsg);
  filter_expr_unref(f);
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

  app_startup();

  configuration = cfg_new(0x0302);
  plugin_load_module("syslogformat", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);


  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("user")), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("daemon")), 0);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("daemon") | facility_bits("user")), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("uucp") | facility_bits("local4")), 0);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_facility_new(0x80000000 | (LOG_USER >> 3)), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_facility_new(0x80000000 | (LOG_DAEMON >> 3)), 0);

  testcase("<2> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("kern")), 1);
  testcase("<2> openvpn[2499]: PTHREAD support initialized", filter_facility_new(0x80000000 | (LOG_KERN >> 3)), 1);

  testcase("<128> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("local0")), 1);
  testcase("<128> openvpn[2499]: PTHREAD support initialized", filter_facility_new(0x80000000 | (LOG_LOCAL0 >> 3)), 1);
  testcase("<32> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("local1")), 0);
  testcase("<32> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("auth")), 1);
  testcase("<32> openvpn[2499]: PTHREAD support initialized", filter_facility_new(0x80000000 | (LOG_AUTH >> 3)), 1);
#ifdef LOG_AUTHPRIV
  testcase("<80> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("authpriv")), 1);
  testcase("<80> openvpn[2499]: PTHREAD support initialized", filter_facility_new(0x80000000 | (LOG_AUTHPRIV >> 3)), 1);
#endif

  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("debug") | level_bits("emerg")), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("emerg")), 0);

  testcase("<8> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), 1);
  testcase("<9> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), 1);
  testcase("<10> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), 1);
  testcase("<11> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), 0);
  testcase("<12> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), 0);
  testcase("<13> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), 0);
  testcase("<14> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), 0);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), 0);

  testcase("<8> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), 0);
  testcase("<9> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), 0);
  testcase("<10> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), 0);
  testcase("<11> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), 0);
  testcase("<12> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), 0);
  testcase("<13> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), 1);
  testcase("<14> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), 1);

  for (i = 0; i < 10; i++)
    {
      testcase("<0> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("emerg")), 1);
      testcase("<1> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("alert")), 1);
      testcase("<2> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("crit")), 1);
      testcase("<3> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("err")), 1);
      testcase("<4> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("warning")), 1);
      testcase("<5> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("notice")), 1);
      testcase("<6> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("info")), 1);
      testcase("<7> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("debug")), 1);
    }

  testcase("<15> openvpn[2499]: PTHREAD support initialized", create_posix_regexp_filter(LM_V_PROGRAM, "^openvpn$", 0), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", create_posix_regexp_filter(LM_V_PROGRAM, "^open$", 0), 0);
  fprintf(stderr, "One \"invalid regular expression\" message is to be expected\n");
  TEST_ASSERT(create_posix_regexp_filter(LM_V_PROGRAM, "((", 0) == NULL);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_posix_regexp_filter(LM_V_HOST, "^host$", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_posix_regexp_filter(LM_V_HOST, "^hos$", 0), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_posix_regexp_filter(LM_V_HOST, "pthread", 0), 0);
  fprintf(stderr, "One \"invalid regular expressions\" message is to be expected\n");
  TEST_ASSERT(create_posix_regexp_filter(LM_V_HOST, "((", 0) == NULL);


  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_posix_regexp_filter(LM_V_MESSAGE, "^PTHREAD ", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_posix_regexp_filter(LM_V_MESSAGE, "PTHREAD s", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_posix_regexp_filter(LM_V_MESSAGE, "^PTHREAD$", 0), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_posix_regexp_filter(LM_V_MESSAGE, "(?i)pthread", 0), 1);


  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_posix_regexp_match(" PTHREAD ", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_posix_regexp_match("^openvpn\\[2499\\]: PTHREAD", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_posix_regexp_match("^PTHREAD$", 0), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_posix_regexp_match("(?i)pthread", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_posix_regexp_match("pthread", LMF_ICASE), 1);

  fprintf(stderr, "One \"invalid regular expression\" message is to be expected\n");
  TEST_ASSERT(create_posix_regexp_match("((", 0) == NULL);


  sender_saddr = g_sockaddr_inet_new("10.10.0.1", 5000);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask_new("10.10.0.0/16"), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask_new("10.10.0.0/24"), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask_new("10.10.10.0/24"), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask_new("0.0.10.10/24"), 0);
  g_sockaddr_unref(sender_saddr);
  sender_saddr = NULL;
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask_new("127.0.0.1/32"), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask_new("127.0.0.2/32"), 0);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_or_new(create_posix_regexp_match(" PTHREAD ", 0), create_posix_regexp_match("PTHREAD", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_or_new(create_posix_regexp_match(" PTHREAD ", 0), create_posix_regexp_match("^PTHREAD$", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_or_new(create_posix_regexp_match("^PTHREAD$", 0), create_posix_regexp_match(" PTHREAD ", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_or_new(create_posix_regexp_match(" PAD ", 0), create_posix_regexp_match("^PTHREAD$", 0)), 0);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_and_new(create_posix_regexp_match(" PTHREAD ", 0), create_posix_regexp_match("PTHREAD", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_and_new(create_posix_regexp_match(" PTHREAD ", 0), create_posix_regexp_match("^PTHREAD$", 0)), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_and_new(create_posix_regexp_match("^PTHREAD$", 0), create_posix_regexp_match(" PTHREAD ", 0)), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_and_new(create_posix_regexp_match(" PAD ", 0), create_posix_regexp_match("^PTHREAD$", 0)), 0);


  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"), create_template("korte"), KW_LT), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"), create_template("korte"), KW_LE), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"), create_template("korte"), KW_EQ), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"), create_template("korte"), KW_NE), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"), create_template("korte"), KW_GE), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"), create_template("korte"), KW_GT), 0);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"), create_template("alma"), KW_LT), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"), create_template("alma"), KW_LE), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"), create_template("alma"), KW_EQ), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"), create_template("alma"), KW_NE), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"), create_template("alma"), KW_GE), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"), create_template("alma"), KW_GT), 1);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"), create_template("alma"), KW_LT), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"), create_template("alma"), KW_LE), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"), create_template("alma"), KW_EQ), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"), create_template("alma"), KW_NE), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"), create_template("alma"), KW_GE), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"), create_template("alma"), KW_GT), 0);


  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", create_posix_regexp_filter(LM_V_MESSAGE, "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "1","a");

  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", create_posix_regexp_filter(LM_V_MESSAGE, "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "0","al fa");

  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", create_posix_regexp_filter(LM_V_MESSAGE, "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "232", NULL);


#if ENABLE_PCRE
  testcase("<15> openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_PROGRAM, "^openvpn$", 0), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_PROGRAM, "^open$", 0), 0);
  fprintf(stderr, "One \"invalid regular expression\" message is to be expected\n");
  TEST_ASSERT(create_pcre_regexp_filter(LM_V_PROGRAM, "((", 0) == NULL);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST, "^host$", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST, "^hos$", 0), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST, "pthread", 0), 0);
  fprintf(stderr, "One \"invalid regular expressions\" message is to be expected\n");
  TEST_ASSERT(create_pcre_regexp_filter(LM_V_HOST, "((", 0) == NULL);

  fprintf(stderr, "One \"invalid regular expressions\" message is to be expected\n");
  TEST_ASSERT(create_posix_regexp_filter(LM_V_HOST, "(?iana", 0) == NULL);

  fprintf(stderr, "One \"invalid regular expressions\" message is to be expected\n");
  TEST_ASSERT(create_pcre_regexp_filter(LM_V_HOST, "(?iana", 0) == NULL);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE, "^PTHREAD ", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE, "PTHREAD s", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE, "^PTHREAD$", 0), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE, "(?i)pthread", 0), 1);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_and_new(create_pcre_regexp_match(" PTHREAD ", 0), create_posix_regexp_match("PTHREAD", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_and_new(create_pcre_regexp_match(" PTHREAD ", 0), create_posix_regexp_match("^PTHREAD$", 0)), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_and_new(create_pcre_regexp_match("^PTHREAD$", 0), create_posix_regexp_match(" PTHREAD ", 0)), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_and_new(create_pcre_regexp_match(" PAD ", 0), create_posix_regexp_match("^PTHREAD$", 0)), 0);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_or_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("PTHREAD", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_or_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_or_new(create_pcre_regexp_match("^PTHREAD$", 0), create_pcre_regexp_match(" PTHREAD ", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_or_new(create_pcre_regexp_match(" PAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), 0);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match(" PTHREAD ", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("^openvpn\\[2499\\]: PTHREAD", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("^PTHREAD$", 0), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("(?i)pthread", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("pthread", LMF_ICASE), 1);

  fprintf(stderr, "One \"invalid regular expression\" message is to be expected\n");
  TEST_ASSERT(create_pcre_regexp_match("((", 0) == NULL);

  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: alma fa", create_pcre_regexp_filter(LM_V_MESSAGE, "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa>fa)", LMF_STORE_MATCHES), 1, "MM","m");
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: alma fa", create_pcre_regexp_filter(LM_V_MESSAGE, "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa>fa)", LMF_STORE_MATCHES), 1, "aaaa", NULL);
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: alma fa", create_pcre_regexp_filter(LM_V_MESSAGE, "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa_name>fa)", LMF_STORE_MATCHES), 1, "fa_name","fa");

  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE, "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "2","l");
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE, "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "0","al fa");
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE, "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "233",NULL);
#endif

  app_shutdown();
  return 0;
}

