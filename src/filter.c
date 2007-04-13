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


gchar *re_matches[RE_MAX_MATCHES];

static void log_filter_rule_free(LogFilterRule *self);

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

LogFilterRule *
log_filter_rule_new(gchar *name, FilterExprNode *expr)
{
  LogFilterRule *self = g_new0(LogFilterRule, 1);
  
  self->ref_cnt = 1;
  self->root = expr;
  self->name = g_string_new(name);
  return self;
}

static void
log_filter_rule_free(LogFilterRule *self)
{
  g_string_free(self->name, TRUE);
  filter_expr_free(self->root);
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
  guint32 fac = msg->pri & LOG_FACMASK;
  guint32 bits = self->valid;
  int i;
  
  if (bits & 0x80000000)
    {
      /* exact number specified */
      return ((bits & ~0x80000000) == fac) ^ s->comp;
    }
  else
    {
      for (i = 0; bits && sl_facilities[i].name; i++, bits >>= 1) 
        {
          if ((bits & 1) && sl_facilities[i].value == fac) 
            {
              return !self->super.comp;
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
  return &self->super;
}

static gboolean
filter_level_eval(FilterExprNode *s, LogMessage *msg)
{
  FilterPri *self = (FilterPri *) s;
  guint32 pri = msg->pri & LOG_PRIMASK;
  
  return (!!(self->valid & (1 << pri))) ^ s->comp;
}

FilterExprNode *
filter_level_new(guint32 levels)
{
  FilterPri *self = g_new0(FilterPri, 1);

  self->super.eval = filter_level_eval;
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
filter_re_eval(FilterRE *self, gchar *str)
{
  regmatch_t matches[RE_MAX_MATCHES];
  gboolean rc;
  gint i;
  
  for (i = 0; i < RE_MAX_MATCHES; i++)
    {
      g_free(re_matches[i]);
      re_matches[i] = NULL;
    }
  rc = !regexec(&self->regex, str, RE_MAX_MATCHES, matches, 0);
  if (rc)
    {
      for (i = 0; i < RE_MAX_MATCHES && matches[i].rm_so != -1; i++)
        {
          gint length = matches[i].rm_eo - matches[i].rm_so;
          re_matches[i] = g_malloc(length + 1);
          memcpy(re_matches[i], &str[matches[i].rm_so], length);
          re_matches[i][length] = 0;
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
  return filter_re_eval((FilterRE *) s, msg->program->str);
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
  return &self->super;
}

static gboolean
filter_host_eval(FilterExprNode *s, LogMessage *msg)
{
  return filter_re_eval((FilterRE *) s, msg->host->str);
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
  return &self->super;
}

static gboolean
filter_match_eval(FilterExprNode *s, LogMessage *msg)
{
  return filter_re_eval((FilterRE *) s, msg->msg->str);
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
  return &self->super;
}

typedef struct _FilterCall
{
  FilterExprNode super;
  GString *rule;
  GlobalConfig *cfg;
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
      msg_error("Referenced filter rule not found",
                evt_tag_str("rule", self->rule->str),
                NULL);
      return 0;
    }
}

static void
filter_call_free(FilterExprNode *s)
{
  FilterCall *self = (FilterCall *) s;
  
  g_string_free(self->rule, TRUE);
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
  return &self->super;
}

