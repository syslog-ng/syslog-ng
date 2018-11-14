/*
 * Copyright (c) 2005-2018 Balabit
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

MsgFormatOptions parse_options;

gint
facility_bits(const gchar *fac)
{
  return 1 << (syslog_name_lookup_facility_by_name(fac) >> 3);
}

gint
level_bits(const gchar *lev)
{
  return 1 << syslog_name_lookup_level_by_name(lev);
}

gint
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


