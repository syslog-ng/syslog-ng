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
