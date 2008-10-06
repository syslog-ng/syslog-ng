/*
 * Copyright (c) 2002-2008 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
  
#include "filter.h"
#include "syslog-names.h"
#include "messages.h"
#include "cfg.h"
#include "logprocess.h"
#include "gsocket.h"
#include "misc.h"

#include <regex.h>
#include <string.h>
#include <stdlib.h>

typedef struct _LogFilterRule
{
  LogProcessRule super;
  FilterExprNode *expr;
} LogFilterRule;

/****************************************************************
 * Filter expression nodes
 ****************************************************************/

gboolean
filter_expr_eval(FilterExprNode *self, LogMessage *msg)
{
  gboolean res;
  
  res = self->eval(self, msg);
  msg_debug("Filter node evaluation result",
            evt_tag_str("filter_result", res ? "match" : "not-match"),
            evt_tag_str("filter_type", self->type),
            NULL);
  return res;
}

void
filter_expr_free(FilterExprNode *self)
{
  if (self->free_fn)
    self->free_fn(self);
  else
    g_free(self);
}


typedef struct _FilterOp
{
  FilterExprNode super;
  FilterExprNode *left, *right;
} FilterOp;

static void
fop_free(FilterExprNode *s)
{
  FilterOp *self = (FilterOp *) s;
  
  filter_expr_free(self->left);
  filter_expr_free(self->right);
  g_free(self);
}

static gboolean
fop_or_eval(FilterExprNode *s, LogMessage *msg)
{
  FilterOp *self = (FilterOp *) s;
  
  return (filter_expr_eval(self->left, msg) || filter_expr_eval(self->right, msg)) ^ s->comp;
}

FilterExprNode *
fop_or_new(FilterExprNode *e1, FilterExprNode *e2)
{
  FilterOp *self = g_new0(FilterOp, 1);
  
  self->super.eval = fop_or_eval;
  self->super.free_fn = fop_free;
  self->super.modify = e1->modify || e2->modify;
  self->left = e1;
  self->right = e2;
  self->super.type = "OR";
  return &self->super;
}

static gboolean
fop_and_eval(FilterExprNode *s, LogMessage *msg)
{
  FilterOp *self = (FilterOp *) s;
  
  return (filter_expr_eval(self->left, msg) && filter_expr_eval(self->right, msg)) ^ s->comp;
}

FilterExprNode *
fop_and_new(FilterExprNode *e1, FilterExprNode *e2)
{
  FilterOp *self = g_new0(FilterOp, 1);
  
  self->super.eval = fop_and_eval;
  self->super.free_fn = fop_free;
  self->super.modify = e1->modify || e2->modify;
  self->left = e1;
  self->right = e2;
  self->super.type = "AND";
  return &self->super;
}

typedef struct _FilterPri
{
  FilterExprNode super;
  guint32 valid;
} FilterPri;

static gboolean
filter_facility_eval(FilterExprNode *s, LogMessage *msg)
{
  FilterPri *self = (FilterPri *) s;
  guint32 fac_num = (msg->pri & LOG_FACMASK) >> 3;
  
  if (G_UNLIKELY(self->valid & 0x80000000))
    {
      /* exact number specified */
      return ((self->valid & ~0x80000000) == fac_num) ^ s->comp;
    }
  else
    {
      return !!(self->valid & (1 << fac_num)) ^ self->super.comp;
    }
  return self->super.comp;
}

FilterExprNode *
filter_facility_new(guint32 facilities)
{
  FilterPri *self = g_new0(FilterPri, 1);

  self->super.eval = filter_facility_eval;
  self->valid = facilities;
  self->super.type = "facility";
  return &self->super;
}

static gboolean
filter_level_eval(FilterExprNode *s, LogMessage *msg)
{
  FilterPri *self = (FilterPri *) s;
  guint32 pri = msg->pri & LOG_PRIMASK;
  
  return !!((1 << pri) & self->valid) ^ self->super.comp;
}

FilterExprNode *
filter_level_new(guint32 levels)
{
  FilterPri *self = g_new0(FilterPri, 1);

  self->super.eval = filter_level_eval;
  self->valid = levels;
  self->super.type = "level";
  return &self->super;
}

static gboolean
filter_re_eval_string(FilterExprNode *s, LogMessage *msg, const gchar *str, gssize str_len)
{
  FilterRE *self = (FilterRE *) s;

  if (str_len < 0)
    str_len = strlen(str);

  return log_matcher_match(self->matcher, msg, self->value_name, str, str_len) ^ self->super.comp;
}

static gboolean
filter_re_eval(FilterExprNode *s, LogMessage *msg)
{
  FilterRE *self = (FilterRE *) s;
  gchar *value;
  gssize len;
  
  value = log_msg_get_value(msg, self->value_name, &len);
  
  value = APPEND_ZERO(value, len);
  return filter_re_eval_string(s, msg, value, len);
}


static void
filter_re_free(FilterExprNode *s)
{
  FilterRE *self = (FilterRE *) s;
  
  log_matcher_free(self->matcher);
  log_msg_free_value_name(self->value_name);
  g_free(s);
}

void
filter_re_set_matcher(FilterRE *self, LogMatcher *matcher)
{
  gint flags = 0;
  if(self->matcher)
    {
      /* save the flags to use them in the new matcher */
      flags = self->matcher->flags;
      log_matcher_free(self->matcher);
    }
   self->matcher = matcher;

   filter_re_set_flags(self, flags);
}

void
filter_re_set_flags(FilterRE *self, gint flags)
{
  /* if there is only a flags() param, we must crete the default matcher*/
  if(!self->matcher)
    self->matcher = log_matcher_posix_re_new();
  if (flags & LMF_STORE_MATCHES)
    self->super.modify = TRUE;
  log_matcher_set_flags(self->matcher, flags | LMF_MATCH_ONLY);
}

gboolean
filter_re_set_regexp(FilterRE *self, gchar *re)
{
  if(!self->matcher)
    self->matcher = log_matcher_posix_re_new();

  return log_matcher_compile(self->matcher, re);
}

FilterExprNode *
filter_re_new(const gchar *value_name)
{
  FilterRE *self = g_new0(FilterRE, 1);

  self->value_name = value_name;
  self->super.eval = filter_re_eval;
  self->super.free_fn = filter_re_free;
  return &self->super;
}

static gboolean
filter_match_eval(FilterExprNode *s, LogMessage *msg)
{
  FilterRE *self = (FilterRE *) s;
  gchar *str;
  gboolean res;

  if (G_UNLIKELY(self->value_name == 0))
    {
      /* compatibility mode */
      str = g_strdup_printf("%s%s%s%s: %s", 
                            msg->program, 
                            msg->pid_len > 0 ? "[" : "", 
                            msg->pid,
                            msg->pid_len > 0 ? "]" : "",
                            msg->message);
      res = filter_re_eval_string(s, msg, str, -1);
      g_free(str);
    }
  else
    {
      res = filter_re_eval(s, msg);
    }
  return res;
}

FilterExprNode *
filter_match_new()
{
  FilterRE *self = g_new0(FilterRE, 1);
  self->super.free_fn = filter_re_free;
  self->super.eval = filter_match_eval;
  return &self->super;
}

typedef struct _FilterCall
{
  FilterExprNode super;
  GString *rule;
  GlobalConfig *cfg;
  gboolean error_logged;
} FilterCall;

static gboolean
filter_call_eval(FilterExprNode *s, LogMessage *msg)
{
  FilterCall *self = (FilterCall *) s;
  LogFilterRule *rule;
  
  rule = g_hash_table_lookup(self->cfg->filters, self->rule->str);
  
  if (rule)
    {
      return filter_expr_eval(rule->expr, msg) ^ s->comp;
    }
  else
    {
      if (!self->error_logged)
        {
          msg_error("Referenced filter rule not found",
                    evt_tag_str("rule", self->rule->str),
                    NULL);
          self->error_logged = TRUE;
        }
      return 1 ^ s->comp;
    }
}

static void
filter_call_free(FilterExprNode *s)
{
  FilterCall *self = (FilterCall *) s;
  
  g_string_free(self->rule, TRUE);
  g_free((gchar *) self->super.type);
  g_free(self);
}

FilterExprNode *
filter_call_new(gchar *rule, GlobalConfig *cfg)
{
  FilterCall *self = g_new0(FilterCall, 1);
  
  self->super.eval = filter_call_eval;
  self->super.free_fn = filter_call_free;
  self->rule = g_string_new(rule);
  self->cfg = cfg;
  self->super.type = g_strdup_printf("filter(%s)", rule);
  return &self->super;
}


typedef struct _FilterNetmask
{
  FilterExprNode super;
  struct in_addr address;
  struct in_addr netmask;
} FilterNetmask;

static gboolean
filter_netmask_eval(FilterExprNode *s, LogMessage *msg)
{
  FilterNetmask *self = (FilterNetmask *) s;
  struct in_addr addr;
  
  if (msg->saddr && g_sockaddr_inet_check(msg->saddr))
    {
      addr = ((struct sockaddr_in *) &msg->saddr->sa)->sin_addr;
    }
  else if (!msg->saddr || msg->saddr->sa.sa_family == AF_UNIX)
    {
      addr.s_addr = htonl(INADDR_LOOPBACK);
    }
  return ((addr.s_addr & self->netmask.s_addr) == (self->address.s_addr)) ^ s->comp;

}

FilterExprNode *
filter_netmask_new(gchar *cidr)
{
  FilterNetmask *self = g_new0(FilterNetmask, 1);
  gchar buf[32];
  gchar *slash;
  
  slash = strchr(cidr, '/');
  if (strlen(cidr) >= sizeof(buf) || !slash)
    {
      g_inet_aton(cidr, &self->address);
      self->netmask.s_addr = htonl(0xFFFFFFFF);
    }
  else
    {
      strncpy(buf, cidr, slash - cidr + 1);
      buf[slash - cidr] = 0;
      g_inet_aton(buf, &self->address);
      if (strchr(slash + 1, '.'))
        {
          g_inet_aton(slash + 1, &self->netmask);
        }
      else
        {
          gint prefix = strtol(slash + 1, NULL, 10);
          if (prefix == 32)
            self->netmask.s_addr = htonl(0xFFFFFFFF);
          else
            self->netmask.s_addr = htonl(((1 << prefix) - 1) << (32 - prefix));
        }
    }
  self->address.s_addr &= self->netmask.s_addr;
  self->super.eval = filter_netmask_eval;
  return &self->super;
}

static void
log_filter_rule_free(LogProcessRule *s)
{
  LogFilterRule *self = (LogFilterRule *) s;
  
  filter_expr_free(self->expr);
}

gboolean 
log_filter_rule_process(LogProcessRule *s, LogMessage *msg)
{
  LogFilterRule *self = (LogFilterRule *) s;
  gboolean res;
  
  msg_debug("Filter rule evaluation begins",
            evt_tag_str("filter_rule", self->super.name),
            NULL);
  res = filter_expr_eval(self->expr, msg);
  msg_debug("Filter rule evaluation result",
            evt_tag_str("filter_result", res ? "match" : "not-match"),
            evt_tag_str("filter_rule", self->super.name),
            NULL);
  return res;
}

LogProcessRule *
log_filter_rule_new(const gchar *name, FilterExprNode *expr)
{
  LogFilterRule *self = g_new0(LogFilterRule, 1);

  log_process_rule_init(&self->super, name);  
  self->super.process = log_filter_rule_process;
  self->super.free_fn = log_filter_rule_free;
  self->super.modify = expr->modify;
  self->expr = expr;
  return &self->super;
}
