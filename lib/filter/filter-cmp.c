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

#include "filter/filter-cmp.h"
#include "filter/filter-expr-grammar.h"

#include <stdlib.h>
#include <string.h>

#define FCMP_EQ  0x0001
#define FCMP_LT  0x0002
#define FCMP_GT  0x0004
#define FCMP_NUM 0x0010

typedef struct _FilterCmp
{
  FilterExprNode super;
  LogTemplate *left, *right;
  gint cmp_op;
} FilterCmp;

gboolean
fop_cmp_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterCmp *self = (FilterCmp *) s;
  GString *left_buf = g_string_sized_new(32);
  GString *right_buf = g_string_sized_new(32);
  gboolean result = FALSE;
  gint cmp;

  log_template_format_with_context(self->left, msgs, num_msg, NULL, LTZ_LOCAL, 0, NULL, left_buf);
  log_template_format_with_context(self->right, msgs, num_msg, NULL, LTZ_LOCAL, 0, NULL, right_buf);

  if (self->cmp_op & FCMP_NUM)
    {
      gint l, r;

      l = atoi(left_buf->str);
      r = atoi(right_buf->str);
      if (l == r)
        cmp = 0;
      else if (l > r)
        cmp = -1;
      else
        cmp = 1;
    }
  else
    {
      cmp = strcmp(left_buf->str, right_buf->str);
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

  g_string_free(left_buf, TRUE);
  g_string_free(right_buf, TRUE);
  return result ^ s->comp;
}

void
fop_cmp_free(FilterExprNode *s)
{
  FilterCmp *self = (FilterCmp *) s;

  log_template_unref(self->left);
  log_template_unref(self->right);
}

FilterExprNode *
fop_cmp_new(LogTemplate *left, LogTemplate *right, gint op)
{
  FilterCmp *self = g_new0(FilterCmp, 1);

  filter_expr_node_init_instance(&self->super);
  self->super.eval = fop_cmp_eval;
  self->super.free_fn = fop_cmp_free;
  self->left = left;
  self->right = right;
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
