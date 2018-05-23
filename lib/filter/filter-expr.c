/*
 * Copyright (c) 2002-2012 Balabit
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
#include "messages.h"

/****************************************************************
 * Filter expression nodes
 ****************************************************************/

void
filter_expr_node_init_instance(FilterExprNode *self)
{
  self->ref_cnt = 1;
}

/*
 * In case the filter would modify the message the caller has to make sure
 * that the message is writable.  You can always archieve that with
 * filter_expr_eval_root() below, but you have to be on a processing path to
 * do that.
 */
gboolean
filter_expr_eval_with_context(FilterExprNode *self, LogMessage **msg, gint num_msg)
{
  gboolean res;

  g_assert(num_msg > 0);

  res = self->eval(self, msg, num_msg);
  return res;
}

gboolean
filter_expr_eval(FilterExprNode *self, LogMessage *msg)
{
  return filter_expr_eval_with_context(self, &msg, 1);
}

gboolean
filter_expr_eval_root_with_context(FilterExprNode *self, LogMessage **msg, gint num_msg,
                                   const LogPathOptions *path_options)
{
  g_assert(num_msg > 0);

  if (self->modify)
    log_msg_make_writable(&msg[num_msg - 1], path_options);

  return filter_expr_eval_with_context(self, msg, num_msg);
}

gboolean
filter_expr_eval_root(FilterExprNode *self, LogMessage **msg, const LogPathOptions *path_options)
{
  return filter_expr_eval_root_with_context(self, msg, 1, path_options);
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
  if (self && (--self->ref_cnt == 0))
    {
      if (self->free_fn)
        self->free_fn(self);
      g_free(self);
    }
}
