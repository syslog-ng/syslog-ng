/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
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

#include <regex.h>
#include <string.h>
#include <stdlib.h>

static void log_filter_rule_free(LogFilterRule *self);

gboolean 
log_filter_rule_eval(LogFilterRule *self, LogMessage *msg)
{
  gboolean res;
  
  msg_debug("Filter rule evaluation begins",
            evt_tag_str("filter_rule", self->name->str),
            NULL);
  res = filter_expr_eval(self->root, msg);
  msg_debug("Filter rule evaluation result",
            evt_tag_str("filter_result", res ? "match" : "not-match"),
            evt_tag_str("filter_rule", self->name->str),
            NULL);
  return res;
}

LogFilterRule *
log_filter_rule_new(gchar *name, FilterExprNode *expr)
{
  LogFilterRule *self = g_new0(LogFilterRule, 1);
  
  self->ref_cnt = 1;
  self->root = expr;
  self->name = g_string_new(name);
  return self;
}

LogFilterRule *
log_filter_ref(LogFilterRule *self)
{
  self->ref_cnt++;
  return self;
}

void 
log_filter_unref(LogFilterRule *self)
{
  if (--self->ref_cnt == 0)
    {
      log_filter_rule_free(self);
    }
}

static void
log_filter_rule_free(LogFilterRule *self)
{
  g_string_free(self->name, TRUE);
  filter_expr_free(self->root);
  g_free(self);
}

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
  
  return (filter_expr_eval(self->right, msg) || filter_expr_eval(self->left, msg)) ^ s->comp;
}

FilterExprNode *
fop_or_new(FilterExprNode *e1, FilterExprNode *e2)
{
  FilterOp *self = g_new0(FilterOp, 1);
  
  self->super.eval = fop_or_eval;
  self->super.free_fn = fop_free;
  self->left = e1;
  self->right = e2;
  self->super.type = "OR";
  return &self->super;
}

static gboolean
fop_and_eval(FilterExprNode *s, LogMessage *msg)
{
  FilterOp *self = (FilterOp *) s;
  
  return (filter_expr_eval(self->right, msg) && filter_expr_eval(self->left, msg)) ^ s->comp;
}

FilterExprNode *
fop_and_new(FilterExprNode *e1, FilterExprNode *e2)
{
  FilterOp *self = g_new0(FilterOp, 1);
  
  self->super.eval = fop_and_eval;
  self->super.free_fn = fop_free;
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
  guint32 fac = msg->pri & LOG_FACMASK, fac_num = (msg->pri & LOG_FACMASK) >> 3;
  guint32 bits = self->valid;
  int i;
  
  if (G_UNLIKELY(bits & 0x80000000))
    {
      /* exact number specified */
      return ((bits & ~0x80000000) == fac_num) ^ s->comp;
    }
  else
    {
      static gint8 bitpos[32] = 
      { 
        -1, -1, -1, -1, -1, -1, -1, -1, 
        -1, -1, -1, -1, -1, -1, -1, -1, 
        -1, -1, -1, -1, -1, -1, -1, -1, 
        -1, -1, -1, -1, -1, -1, -1, -1
      };
      
      if (G_LIKELY(fac_num < 32 && bitpos[fac_num] != -1))
        {
          if (self->valid & (1 << bitpos[fac_num]))
            return !self->super.comp;
        }
      else
        {
          for (i = 0; bits && sl_facilities[i].name; i++, bits >>= 1) 
            {
              if (sl_facilities[i].value == fac) 
                {
                  if (fac_num < 32)
                    bitpos[fac_num] = i;
                  if (bits & 1)
                    {
                      return !self->super.comp;
                    }
                }
            }
        }
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

typedef struct _FilterRE
{
  FilterExprNode super;
  regex_t regex;
} FilterRE;

static gboolean
filter_re_compile(const char *re, regex_t *regex)
{
  int rc;
  
  rc = regcomp(regex, re, REG_EXTENDED);
  if (rc)
    {
      gchar buf[256];
                      
      regerror(rc, regex, buf, sizeof(buf));
      msg_error("Error compiling regular expression",
                evt_tag_str("re", re),
                evt_tag_str("error", buf),
                NULL);
      return FALSE;
    }
  return TRUE;
}

static gboolean
filter_re_eval(FilterRE *self, LogMessage *msg, gchar *str)
{
  regmatch_t matches[RE_MAX_MATCHES];
  gboolean rc;
  gint i;

  log_msg_clear_matches(msg);  
  rc = !regexec(&self->regex, str, RE_MAX_MATCHES, matches, 0);
  if (rc)
    {
      for (i = 0; i < RE_MAX_MATCHES && matches[i].rm_so != -1; i++)
        {
          gint length = matches[i].rm_eo - matches[i].rm_so;
          msg->re_matches[i] = g_malloc(length + 1);
          memcpy(msg->re_matches[i], &str[matches[i].rm_so], length);
          msg->re_matches[i][length] = 0;
        }
    }
  return rc ^ self->super.comp;
}

static void
filter_re_free(FilterExprNode *s)
{
  FilterRE *self = (FilterRE *) s;
  
  regfree(&self->regex);
  g_free(s);
}

static gboolean
filter_prog_eval(FilterExprNode *s, LogMessage *msg)
{
  return filter_re_eval((FilterRE *) s, msg, msg->program->str);
}

FilterExprNode *
filter_prog_new(gchar *prog)
{
  FilterRE *self = g_new0(FilterRE, 1);
  
  if (!filter_re_compile(prog, &self->regex))
    {
      g_free(self);
      return NULL;
    }
  self->super.eval = filter_prog_eval;
  self->super.free_fn = filter_re_free;
  self->super.type = "program";
  return &self->super;
}

static gboolean
filter_host_eval(FilterExprNode *s, LogMessage *msg)
{
  return filter_re_eval((FilterRE *) s, msg, msg->host->str);
}

FilterExprNode *
filter_host_new(gchar *host)
{
  FilterRE *self = g_new0(FilterRE, 1);
  
  if (!filter_re_compile(host, &self->regex))
    {
      g_free(self);
      return NULL;
    }
  self->super.eval = filter_host_eval;
  self->super.free_fn = filter_re_free;
  self->super.type = "host";
  return &self->super;
}

static gboolean
filter_match_eval(FilterExprNode *s, LogMessage *msg)
{
  return filter_re_eval((FilterRE *) s, msg, msg->msg->str);
}

FilterExprNode *
filter_match_new(gchar *re)
{
  FilterRE *self = g_new0(FilterRE, 1);
  
  if (!filter_re_compile(re, &self->regex))
    {
      g_free(self);
      return NULL;
    }
  self->super.eval = filter_match_eval;
  self->super.free_fn = filter_re_free;
  self->super.type = "match";
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
      return filter_expr_eval(rule->root, msg) ^ s->comp;
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
  
  if (msg->saddr && g_sockaddr_inet_check(msg->saddr))
    {
      struct in_addr *addr = &((struct sockaddr_in *) &msg->saddr->sa)->sin_addr;
      
      return ((addr->s_addr & self->netmask.s_addr) == (self->address.s_addr)) ^ s->comp;
    }
  return s->comp;
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
