/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include "filter/filter-pipe.h"
#include "syslog-names.h"
#include "messages.h"
#include "cfg.h"
#include "gsocket.h"
#include "misc.h"
#include "tags.h"
#include "cfg-tree.h"

#include <regex.h>
#include <string.h>
#include <stdlib.h>

/****************************************************************
 * Filter expression nodes
 ****************************************************************/

void
filter_expr_node_init(FilterExprNode *self)
{
  self->ref_cnt = 1;
}

gboolean
filter_expr_eval_with_context(FilterExprNode *self, LogMessage **msg, gint num_msg)
{
  gboolean res;

  res = self->eval(self, msg, num_msg);
  msg_debug("Filter node evaluation result",
            evt_tag_str("result", res ? "match" : "not-match"),
            evt_tag_str("type", self->type),
            NULL);
  return res;
}

gboolean
filter_expr_eval(FilterExprNode *self, LogMessage *msg)
{
  return filter_expr_eval_with_context(self, &msg, 1);
}

FilterExprNode *
filter_expr_ref(FilterExprNode *self)
{
  self->ref_cnt++;
  return self;
}

void
filter_expr_unref(FilterExprNode *self)
{
  if (--self->ref_cnt == 0)
    {
      if (self->free_fn)
        self->free_fn(self);
      g_free(self);
    }
}

typedef struct _FilterOp
{
  FilterExprNode super;
  FilterExprNode *left, *right;
} FilterOp;

static void
fop_init(FilterExprNode *s, GlobalConfig *cfg)
{
  FilterOp *self = (FilterOp *) s;

  if (self->left && self->left->init)
    self->left->init(self->left, cfg);
  if (self->right && self->right->init)
    self->right->init(self->right, cfg);
}

static void
fop_free(FilterExprNode *s)
{
  FilterOp *self = (FilterOp *) s;

  filter_expr_unref(self->left);
  filter_expr_unref(self->right);
}

static void
fop_init_instance(FilterOp *self)
{
  filter_expr_node_init(&self->super);
  self->super.init = fop_init;
  self->super.free_fn = fop_free;
}

static gboolean
fop_or_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterOp *self = (FilterOp *) s;

  return (filter_expr_eval_with_context(self->left, msgs, num_msg) || filter_expr_eval_with_context(self->right, msgs, num_msg)) ^ s->comp;
}

FilterExprNode *
fop_or_new(FilterExprNode *e1, FilterExprNode *e2)
{
  FilterOp *self = g_new0(FilterOp, 1);

  fop_init_instance(self);
  self->super.eval = fop_or_eval;
  self->super.modify = e1->modify || e2->modify;
  self->left = e1;
  self->right = e2;
  self->super.type = "OR";
  return &self->super;
}

static gboolean
fop_and_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterOp *self = (FilterOp *) s;

  return (filter_expr_eval_with_context(self->left, msgs, num_msg) && filter_expr_eval_with_context(self->right, msgs, num_msg)) ^ s->comp;
}

FilterExprNode *
fop_and_new(FilterExprNode *e1, FilterExprNode *e2)
{
  FilterOp *self = g_new0(FilterOp, 1);

  fop_init_instance(self);
  self->super.eval = fop_and_eval;
  self->super.modify = e1->modify || e2->modify;
  self->left = e1;
  self->right = e2;
  self->super.type = "AND";
  return &self->super;
}

#define FCMP_EQ  0x0001
#define FCMP_LT  0x0002
#define FCMP_GT  0x0004
#define FCMP_NUM 0x0010

typedef struct _FilterCmp
{
  FilterExprNode super;
  LogTemplate *left, *right;
  GString *left_buf, *right_buf;
  gint cmp_op;
} FilterCmp;

gboolean
fop_cmp_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterCmp *self = (FilterCmp *) s;
  gboolean result = FALSE;
  gint cmp;

  log_template_format_with_context(self->left, msgs, num_msg, NULL, LTZ_LOCAL, 0, NULL, self->left_buf);
  log_template_format_with_context(self->right, msgs, num_msg, NULL, LTZ_LOCAL, 0, NULL, self->right_buf);

  if (self->cmp_op & FCMP_NUM)
    {
      gint l, r;

      l = atoi(self->left_buf->str);
      r = atoi(self->right_buf->str);
      if (l == r)
        cmp = 0;
      else if (l > r)
        cmp = -1;
      else
        cmp = 1;
    }
  else
    {
      cmp = strcmp(self->left_buf->str, self->right_buf->str);
    }

  if (cmp == 0)
    {
      result = self->cmp_op & FCMP_EQ;
    }
  else if (cmp < 0)
    {
      result = self->cmp_op & FCMP_LT || self->cmp_op == 0;
    }
  else
    {
      result = self->cmp_op & FCMP_GT || self->cmp_op == 0;
    }
  return result ^ s->comp;
}

void
fop_cmp_free(FilterExprNode *s)
{
  FilterCmp *self = (FilterCmp *) s;

  log_template_unref(self->left);
  log_template_unref(self->right);
  g_string_free(self->left_buf, TRUE);
  g_string_free(self->right_buf, TRUE);
}

FilterExprNode *
fop_cmp_new(LogTemplate *left, LogTemplate *right, gint op)
{
  FilterCmp *self = g_new0(FilterCmp, 1);

  filter_expr_node_init(&self->super);
  self->super.eval = fop_cmp_eval;
  self->super.free_fn = fop_cmp_free;
  self->left = left;
  self->right = right;
  self->left_buf = g_string_sized_new(32);
  self->right_buf = g_string_sized_new(32);
  self->super.type = "CMP";

  switch (op)
    {
    case KW_NUM_LT:
      self->cmp_op = FCMP_NUM;
    case KW_LT:
      self->cmp_op = FCMP_LT;
      break;

    case KW_NUM_LE:
      self->cmp_op = FCMP_NUM;
    case KW_LE:
      self->cmp_op = FCMP_LT | FCMP_EQ;
      break;

    case KW_NUM_EQ:
      self->cmp_op = FCMP_NUM;
    case KW_EQ:
      self->cmp_op = FCMP_EQ;
      break;

    case KW_NUM_NE:
      self->cmp_op = FCMP_NUM;
    case KW_NE:
      self->cmp_op = 0;
      break;

    case KW_NUM_GE:
      self->cmp_op = FCMP_NUM;
    case KW_GE:
      self->cmp_op = FCMP_GT | FCMP_EQ;
      break;

    case KW_NUM_GT:
      self->cmp_op = FCMP_NUM;
    case KW_GT:
      self->cmp_op = FCMP_GT;
      break;
    }
  return &self->super;
}


typedef struct _FilterPri
{
  FilterExprNode super;
  guint32 valid;
} FilterPri;

static gboolean
filter_facility_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterPri *self = (FilterPri *) s;
  LogMessage *msg = msgs[0];
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

  filter_expr_node_init(&self->super);
  self->super.eval = filter_facility_eval;
  self->valid = facilities;
  self->super.type = "facility";
  return &self->super;
}

static gboolean
filter_level_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterPri *self = (FilterPri *) s;
  LogMessage *msg = msgs[0];
  guint32 pri = msg->pri & LOG_PRIMASK;


  return !!((1 << pri) & self->valid) ^ self->super.comp;
}

FilterExprNode *
filter_level_new(guint32 levels)
{
  FilterPri *self = g_new0(FilterPri, 1);

  filter_expr_node_init(&self->super);
  self->super.eval = filter_level_eval;
  self->valid = levels;
  self->super.type = "level";
  return &self->super;
}

static gboolean
filter_re_eval_string(FilterExprNode *s, LogMessage *msg, gint value_handle, const gchar *str, gssize str_len)
{
  FilterRE *self = (FilterRE *) s;

  if (str_len < 0)
    str_len = strlen(str);

  return log_matcher_match(self->matcher, msg, value_handle, str, str_len) ^ self->super.comp;
}

static gboolean
filter_re_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterRE *self = (FilterRE *) s;
  const gchar *value;
  LogMessage *msg = msgs[0];
  gssize len = 0;

  value = log_msg_get_value(msg, self->value_handle, &len);

  APPEND_ZERO(value, value, len);
  return filter_re_eval_string(s, msg, self->value_handle, value, len);
}


static void
filter_re_free(FilterExprNode *s)
{
  FilterRE *self = (FilterRE *) s;

  log_matcher_unref(self->matcher);
}

void
filter_re_set_matcher(FilterRE *self, LogMatcher *matcher)
{
  gint flags = 0;
  if(self->matcher)
    {
      /* save the flags to use them in the new matcher */
      flags = self->matcher->flags;
      log_matcher_unref(self->matcher);
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
filter_re_new(NVHandle value_handle)
{
  FilterRE *self = g_new0(FilterRE, 1);

  filter_expr_node_init(&self->super);
  self->value_handle = value_handle;
  self->super.eval = filter_re_eval;
  self->super.free_fn = filter_re_free;
  return &self->super;
}

static gboolean
filter_match_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterRE *self = (FilterRE *) s;
  gchar *str;
  gboolean res;
  LogMessage *msg = msgs[0];

  if (G_UNLIKELY(!self->value_handle))
    {
      const gchar *pid;
      gssize pid_len;

      pid = log_msg_get_value(msg, LM_V_PID, &pid_len);

      /* compatibility mode */
      str = g_strdup_printf("%s%s%s%s: %s",
                            log_msg_get_value(msg, LM_V_PROGRAM, NULL),
                            pid_len > 0 ? "[" : "",
                            pid,
                            pid_len > 0 ? "]" : "",
                            log_msg_get_value(msg, LM_V_MESSAGE, NULL));
      res = filter_re_eval_string(s, msg, LM_V_NONE, str, -1);
      g_free(str);
    }
  else
    {
      res = filter_re_eval(s, msgs, num_msg);
    }
  return res;
}

FilterExprNode *
filter_match_new()
{
  FilterRE *self = g_new0(FilterRE, 1);

  filter_expr_node_init(&self->super);
  self->super.free_fn = filter_re_free;
  self->super.eval = filter_match_eval;
  return &self->super;
}

typedef struct _FilterCall
{
  FilterExprNode super;
  FilterExprNode *filter_expr;
  gchar *rule;
} FilterCall;

static gboolean
filter_call_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterCall *self = (FilterCall *) s;

  if (self->filter_expr)
    {
      /* rule is assumed to contain a single filter pipe */

      return filter_expr_eval_with_context(self->filter_expr, msgs, num_msg) ^ s->comp;
    }
  else
    {
      /* an unreferenced filter() expression never matches unless explicitly negated */
      return (0 ^ s->comp);
    }
}

static void
filter_call_init(FilterExprNode *s, GlobalConfig *cfg)
{
  FilterCall *self = (FilterCall *) s;
  LogExprNode *rule;

  rule = cfg_tree_get_object(&cfg->tree, ENC_FILTER, self->rule);
  if (rule)
    {
      /* this is quite fragile and would break whenever the parsing code in
       * cfg-grammar.y changes to parse a filter rule.  We assume that a
       * filter rule has a single child, which contains a LogFilterPipe
       * instance as its object. */


      self->filter_expr = ((LogFilterPipe *) rule->children->object)->expr;
    }
  else
    {
      msg_error("Referenced filter rule not found in filter() expression",
                evt_tag_str("rule", self->rule),
                NULL);
    }
}

static void
filter_call_free(FilterExprNode *s)
{
  FilterCall *self = (FilterCall *) s;

  g_free((gchar *) self->super.type);
  g_free(self->rule);
}

FilterExprNode *
filter_call_new(gchar *rule, GlobalConfig *cfg)
{
  FilterCall *self = g_new0(FilterCall, 1);

  filter_expr_node_init(&self->super);
  self->super.init = filter_call_init;
  self->super.eval = filter_call_eval;
  self->super.free_fn = filter_call_free;
  self->super.type = g_strdup_printf("filter(%s)", rule);
  self->rule = g_strdup(rule);
  return &self->super;
}


typedef struct _FilterNetmask
{
  FilterExprNode super;
  struct in_addr address;
  struct in_addr netmask;
} FilterNetmask;

static gboolean
filter_netmask_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterNetmask *self = (FilterNetmask *) s;
  struct in_addr addr;
  LogMessage *msg = msgs[0];

  if (msg->saddr && g_sockaddr_inet_check(msg->saddr))
    {
      addr = ((struct sockaddr_in *) &msg->saddr->sa)->sin_addr;
    }
  else if (!msg->saddr || msg->saddr->sa.sa_family == AF_UNIX)
    {
      addr.s_addr = htonl(INADDR_LOOPBACK);
    }
  else
    {
      /* no address information, return FALSE */
      return s->comp;
    }
  return ((addr.s_addr & self->netmask.s_addr) == (self->address.s_addr)) ^ s->comp;

}

FilterExprNode *
filter_netmask_new(gchar *cidr)
{
  FilterNetmask *self = g_new0(FilterNetmask, 1);
  gchar buf[32];
  gchar *slash;

  filter_expr_node_init(&self->super);
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

typedef struct _FilterTags
{
  FilterExprNode super;
  GArray *tags;
} FilterTags;

static gboolean
filter_tags_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterTags *self = (FilterTags *)s;
  LogMessage *msg = msgs[0];
  gint i;

  for (i = 0; i < self->tags->len; i++)
    {
      if (log_msg_is_tag_by_id(msg, g_array_index(self->tags, LogTagId, i)))
        return TRUE ^ s->comp;
    }

  return FALSE ^ s->comp;
}

void
filter_tags_add(FilterExprNode *s, GList *tags)
{
  FilterTags *self = (FilterTags *)s;
  LogTagId id;

  while (tags)
    {
      id = log_tags_get_by_name((gchar *) tags->data);
      g_free(tags->data);
      tags = g_list_delete_link(tags, tags);
      g_array_append_val(self->tags, id);
    }
}

static void
filter_tags_free(FilterExprNode *s)
{
  FilterTags *self = (FilterTags *)s;

  g_array_free(self->tags, TRUE);
}

FilterExprNode *
filter_tags_new(GList *tags)
{
  FilterTags *self = g_new0(FilterTags, 1);

  filter_expr_node_init(&self->super);
  self->tags = g_array_new(FALSE, FALSE, sizeof(LogTagId));

  filter_tags_add(&self->super, tags);

  self->super.eval = filter_tags_eval;
  self->super.free_fn = filter_tags_free;
  return &self->super;
}
