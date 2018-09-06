/*
 * Copyright (c) 2005-2015 Balabit
 * Copyright (c) 2005-2015 Balázs Scheidler <balazs.scheidler@balabit.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "filter/filter-expr.h"
#include "filter/filter-expr-grammar.h"
#include "filter/filter-netmask.h"
#include "filter/filter-netmask6.h"
#include "filter/filter-op.h"
#include "filter/filter-cmp.h"
#include "filter/filter-tags.h"
#include "filter/filter-re.h"
#include "filter/filter-pri.h"
#include "cfg.h"
#include "messages.h"
#include "syslog-names.h"
#include "logmsg/logmsg.h"
#include "apphook.h"
#include "plugin.h"


#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int debug = 1;
MsgFormatOptions parse_options;

static gint
facility_bits(const gchar *fac)
{
  return 1 << (syslog_name_lookup_facility_by_name(fac) >> 3);
}

static gint
level_bits(const gchar *lev)
{
  return 1 << syslog_name_lookup_level_by_name(lev);
}

static gint
level_range(const gchar *from, const gchar *to)
{
  int r1, r2;

  r1 = syslog_name_lookup_level_by_name(from);
  r2 = syslog_name_lookup_level_by_name(to);
  return syslog_make_range(r1, r2);
}

FilterExprNode *
compile_pattern(FilterRE *f, const gchar *regexp, const gchar *type, gint flags)
{
  gboolean result;

  log_matcher_options_defaults(&f->matcher_options);
  f->matcher_options.flags = flags;
  log_matcher_options_set_type(&f->matcher_options, type);

  result = filter_re_compile_pattern(f, configuration, regexp, NULL);

  if (result)
    return &f->super;

  filter_expr_unref(&f->super);
  return NULL;
}

FilterExprNode *
create_pcre_regexp_filter(gint field, const gchar *regexp, gint flags)
{
  return compile_pattern(filter_re_new(field), regexp, "pcre", flags);
}

FilterExprNode *
create_pcre_regexp_match(const gchar *regexp, gint flags)
{
  return compile_pattern(filter_match_new(), regexp, "pcre", flags);
}

LogTemplate *
create_template(const gchar *template)
{
  LogTemplate *t;

  t = log_template_new(configuration, NULL);
  log_template_compile(t, template, NULL);
  return t;
}

#if SYSLOG_NG_ENABLE_IPV6
static gboolean
_is_ipv6(const gchar *sockaddr)
{
  return (NULL != strchr(sockaddr, ':'));
}
#endif

static GSockAddr *
_get_sockaddr(const gchar *sockaddr)
{
  if (!sockaddr)
    return NULL;

#if SYSLOG_NG_ENABLE_IPV6
  if (_is_ipv6(sockaddr))
    {
      return g_sockaddr_inet6_new(sockaddr, 5000);
    }
#endif
  return g_sockaddr_inet_new(sockaddr, 5000);
}

void
testcase_with_socket(const gchar *msg, const gchar *sockaddr,
                     FilterExprNode *f,
                     gboolean expected_result)
{
  LogMessage *logmsg;
  gboolean res;

  filter_expr_init(f, configuration);

  logmsg = log_msg_new(msg, strlen(msg), NULL, &parse_options);
  logmsg->saddr = _get_sockaddr(sockaddr);

  res = filter_expr_eval(f, logmsg);
  if (res != expected_result)
    {
      fprintf(stderr, "Filter test failed; msg='%s'\n", msg);
      exit(1);
    }

  f->comp = 1;
  res = filter_expr_eval(f, logmsg);
  if (res != !expected_result)
    {
      fprintf(stderr, "Filter test failed (negated); msg='%s'\n", msg);
      exit(1);
    }

  log_msg_unref(logmsg);
  filter_expr_unref(f);
}

void
testcase(const gchar *msg,
         FilterExprNode *f,
         gboolean expected_result)
{
  testcase_with_socket(msg, NULL, f, expected_result);
}

void
testcase_with_backref_chk(const gchar *msg,
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
  gssize length;
  NVHandle nonasciiz = log_msg_get_value_handle("NON-ASCIIZ");
  gssize msglen;
  gchar buf[1024];

  logmsg = log_msg_new(msg, strlen(msg), NULL, &parse_options);
  logmsg->saddr = g_sockaddr_inet_new("10.10.0.1", 5000);

  /* NOTE: we test how our filters cope with non-zero terminated values. We don't change message_len, only the value */
  g_snprintf(buf, sizeof(buf), "%sAAAAAAAAAAAA", log_msg_get_value(logmsg, LM_V_MESSAGE, &msglen));
  log_msg_set_value_by_name(logmsg, "MESSAGE2", buf, -1);

  /* add a non-zero terminated indirect value which contains the whole message */
  log_msg_set_value_indirect(logmsg, nonasciiz, log_msg_get_value_handle("MESSAGE2"), 0, 0, msglen);

  nv_table = nv_table_ref(logmsg->payload);
  res = filter_expr_eval(f, logmsg);
  if (res != expected_result)
    {
      fprintf(stderr, "Filter test failed; msg='%s'\n", msg);
      exit(1);
    }
  nv_table_unref(nv_table);
  f->comp = 1;

  nv_table = nv_table_ref(logmsg->payload);
  res = filter_expr_eval(f, logmsg);
  if (res != !expected_result)
    {
      fprintf(stderr, "Filter test failed (negated); msg='%s'\n", msg);
      exit(1);
    }

  value_msg = log_msg_get_value_by_name(logmsg, name, &length);
  nv_table_unref(nv_table);
  if(value == NULL || value[0] == 0)
    {
      if (value_msg != NULL && value_msg[0] != 0)
        {
          fprintf(stderr, "Filter test failed (NULL value chk); msg='%s', expected_value='%s', value_in_msg='%s'",
                  msg, value, value_msg);
          exit(1);
        }
    }
  else
    {
      if (strncmp(value_msg, value, length) != 0)
        {
          fprintf(stderr, "Filter test failed (value chk); msg='%s', expected_value='%s', value_in_msg='%s'",
                  msg, value, value_msg);
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

void
setup(void)
{
  app_startup();

  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "syslogformat");
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
}

void
teardown(void)
{
  app_shutdown();
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  setup();

  TEST_ASSERT(create_pcre_regexp_filter(LM_V_PROGRAM, "((", 0) == NULL);
  TEST_ASSERT(create_pcre_regexp_filter(LM_V_HOST, "((", 0) == NULL);
  TEST_ASSERT(create_pcre_regexp_match("((", 0) == NULL);
  TEST_ASSERT(create_pcre_regexp_filter(LM_V_PROGRAM, "((", 0) == NULL);
  TEST_ASSERT(create_pcre_regexp_filter(LM_V_HOST, "((", 0) == NULL);
  TEST_ASSERT(create_pcre_regexp_filter(LM_V_HOST, "(?iana", 0) == NULL);
  TEST_ASSERT(create_pcre_regexp_filter(LM_V_HOST, "(?iana", 0) == NULL);
  TEST_ASSERT(create_pcre_regexp_match("((", 0) == NULL);


  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "1","a");
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "0","al fa");
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "232", NULL);
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: alma fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa>fa)", LMF_STORE_MATCHES), 1, "MM","m");
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: alma fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa>fa)", LMF_STORE_MATCHES), 1, "aaaa", NULL);
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: alma fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa_name>fa)", LMF_STORE_MATCHES), 1, "fa_name","fa");
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "2","l");
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "0","al fa");
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "233",NULL);


  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("user")), TRUE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("daemon")), FALSE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized",
           filter_facility_new(facility_bits("daemon") | facility_bits("user")), TRUE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized",
           filter_facility_new(facility_bits("uucp") | facility_bits("local4")), FALSE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_facility_new(0x80000000 | (LOG_USER >> 3)), TRUE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_facility_new(0x80000000 | (LOG_DAEMON >> 3)), FALSE);

  testcase("<2> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("kern")), TRUE);
  testcase("<2> openvpn[2499]: PTHREAD support initialized", filter_facility_new(0x80000000 | (LOG_KERN >> 3)), TRUE);

  testcase("<128> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("local0")), TRUE);
  testcase("<128> openvpn[2499]: PTHREAD support initialized", filter_facility_new(0x80000000 | (LOG_LOCAL0 >> 3)), TRUE);
  testcase("<32> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("local1")), FALSE);
  testcase("<32> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("auth")), TRUE);
  testcase("<32> openvpn[2499]: PTHREAD support initialized", filter_facility_new(0x80000000 | (LOG_AUTH >> 3)), TRUE);
#ifdef LOG_AUTHPRIV
  testcase("<80> openvpn[2499]: PTHREAD support initialized", filter_facility_new(facility_bits("authpriv")), TRUE);
  testcase("<80> openvpn[2499]: PTHREAD support initialized", filter_facility_new(0x80000000 | (LOG_AUTHPRIV >> 3)),
           TRUE);
#endif

  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("debug") | level_bits("emerg")),
           TRUE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("emerg")), FALSE);

  testcase("<8> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), TRUE);
  testcase("<9> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), TRUE);
  testcase("<10> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), TRUE);
  testcase("<11> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), FALSE);
  testcase("<12> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), FALSE);
  testcase("<13> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), FALSE);
  testcase("<14> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), FALSE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), FALSE);

  testcase("<8> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), FALSE);
  testcase("<9> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), FALSE);
  testcase("<10> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), FALSE);
  testcase("<11> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), FALSE);
  testcase("<12> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), FALSE);
  testcase("<13> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), TRUE);
  testcase("<14> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), TRUE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), TRUE);

  testcase("<0> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("emerg")), TRUE);
  testcase("<1> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("alert")), TRUE);
  testcase("<2> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("crit")), TRUE);
  testcase("<3> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("err")), TRUE);
  testcase("<4> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("warning")), TRUE);
  testcase("<5> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("notice")), TRUE);
  testcase("<6> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("info")), TRUE);
  testcase("<7> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("debug")), TRUE);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_PROGRAM, "^openvpn$", 0),
           TRUE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_PROGRAM, "^open$", 0),
           FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST,
           "^host$", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST,
           "^hos$", 0), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST,
           "pthread", 0), FALSE);


  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "^PTHREAD ", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "PTHREAD s", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "^PTHREAD$", 0), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "(?i)pthread", 0), TRUE);


  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match(" PTHREAD ",
           0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           create_pcre_regexp_match("^openvpn\\[2499\\]: PTHREAD", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("^PTHREAD$",
           0), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("(?i)pthread",
           0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("pthread",
           LMF_ICASE), TRUE);


#if SYSLOG_NG_ENABLE_IPV6
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("::/1"), TRUE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348",
                       filter_netmask6_new("2001:db80:85a3:8d30:1319:8a2e::/95"), TRUE);

  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348",
                       filter_netmask6_new("2001:db80:85a3:8d30:1319:8a2e:3700:7348/60"), TRUE);

  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348",
                       filter_netmask6_new("2001:db80:85a3:8d30:1319:8a2e:3700::/114"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348",
                       filter_netmask6_new("::85a3:8d30:1319:8a2e:3700::/114"), FALSE);

  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("aaaaaa/32"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("/8"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("::"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new(""), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("::1/8"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("::1/128"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("::2/32"), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("aaaaaa/32"),
           FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("/8"), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new(""), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::1"), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::/32"), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::1/8"), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::1/128"), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::/16"), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::/599"), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::/aaa"), FALSE);
#endif

  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", "10.10.0.1",
                       filter_netmask_new("10.10.0.0/16"), TRUE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", "10.10.0.1",
                       filter_netmask_new("10.10.0.0/24"), TRUE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", "10.10.0.1",
                       filter_netmask_new("10.10.10.0/24"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", "10.10.0.1",
                       filter_netmask_new("0.0.10.10/24"), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask_new("127.0.0.1/32"),
           TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask_new("127.0.0.2/32"),
           FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("PTHREAD", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match("^PTHREAD$", 0), create_pcre_regexp_match(" PTHREAD ", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match(" PAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("PTHREAD", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match("^PTHREAD$", 0), create_pcre_regexp_match(" PTHREAD ", 0)), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match(" PAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), FALSE);

  /* LEVEL_NUM is 7 */
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("7"), KW_NUM_EQ), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("5"), KW_NUM_NE), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("8"), KW_NUM_LT), TRUE);

  /* 7 < 10 is TRUE */
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("10"), KW_NUM_LT), TRUE);
  /* 7 lt 10 is FALSE as 10 orders lower when interpreted as a string */
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("10"), KW_LT), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("5"), KW_NUM_GT), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("7"), KW_NUM_GE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("7"), KW_NUM_LE), TRUE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("korte"), KW_LT), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("korte"), KW_LE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("korte"), KW_EQ), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("korte"), KW_NE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("korte"), KW_GE), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("korte"), KW_GT), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"),
           create_template("alma"), KW_LT), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"),
           create_template("alma"), KW_LE), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"),
           create_template("alma"), KW_EQ), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"),
           create_template("alma"), KW_NE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"),
           create_template("alma"), KW_GE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"),
           create_template("alma"), KW_GT), TRUE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("alma"), KW_LT), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("alma"), KW_LE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("alma"), KW_EQ), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("alma"), KW_NE), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("alma"), KW_GE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("alma"), KW_GT), FALSE);




  testcase("<15> openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_PROGRAM, "^openvpn$", 0),
           TRUE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_PROGRAM, "^open$", 0),
           FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST,
           "^host$", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST,
           "^hos$", 0), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST,
           "pthread", 0), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "^PTHREAD ", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "PTHREAD s", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "^PTHREAD$", 0), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "(?i)pthread", 0), TRUE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("PTHREAD", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match("^PTHREAD$", 0), create_pcre_regexp_match(" PTHREAD ", 0)), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match(" PAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("PTHREAD", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match("^PTHREAD$", 0), create_pcre_regexp_match(" PTHREAD ", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match(" PAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match(" PTHREAD ",
           0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           create_pcre_regexp_match("^openvpn\\[2499\\]: PTHREAD", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("^PTHREAD$",
           0), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("(?i)pthread",
           0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("pthread",
           LMF_ICASE), TRUE);

  teardown();

  return 0;
}
