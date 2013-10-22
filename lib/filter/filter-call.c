/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#include "filter-call.h"
#include "cfg.h"
#include "filter-pipe.h"

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

  filter_expr_node_init_instance(&self->super);
  self->super.init = filter_call_init;
  self->super.eval = filter_call_eval;
  self->super.free_fn = filter_call_free;
  self->super.type = g_strdup_printf("filter(%s)", rule);
  self->rule = g_strdup(rule);
  return &self->super;
}
