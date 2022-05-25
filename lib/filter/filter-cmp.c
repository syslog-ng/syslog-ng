/*
 * Copyright (c) 2002-2013 Balabit
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
#include "scratch-buffers.h"

#include <stdlib.h>
#include <string.h>

typedef struct _FilterCmp
{
  FilterExprNode super;
  LogTemplate *left, *right;
  gint compare_mode;
} FilterCmp;

static gint
fop_compare_numeric(const gchar *left, const gchar *right)
{
  gint l = atoi(left);
  gint r = atoi(right);
  if (l == r)
    return 0;
  else if (l < r)
    return -1;

  return 1;
}

static gint
fop_compare_string(const gchar *left, const gchar *right)
{
  return strcmp(left, right);
}

static gint
fop_compare(FilterCmp *self, const gchar *left, const gchar *right)
{
  if (self->compare_mode & FCMP_NUM_BASED)
    return fop_compare_numeric(left, right);
  else
    return fop_compare_string(left, right);
}

static gboolean
fop_cmp_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg, LogTemplateEvalOptions *options)
{
  FilterCmp *self = (FilterCmp *) s;

  ScratchBuffersMarker marker;
  GString *left_buf = scratch_buffers_alloc_and_mark(&marker);
  GString *right_buf = scratch_buffers_alloc();

  log_template_format_with_context(self->left, msgs, num_msg, options, left_buf);
  log_template_format_with_context(self->right, msgs, num_msg, options, right_buf);

  gboolean result = FALSE;

  gint cmp = fop_compare(self, left_buf->str, right_buf->str);

  if (cmp == 0)
    {
      result = self->compare_mode & FCMP_EQ;
    }
  else if (cmp < 0)
    {
      result = !!(self->compare_mode & FCMP_LT);
    }
  else
    {
      result = !!(self->compare_mode & FCMP_GT);
    }

  msg_trace("cmp() evaluation started",
            evt_tag_str("left", left_buf->str),
            evt_tag_str("operator", self->super.type),
            evt_tag_str("right", right_buf->str),
            evt_tag_msg_reference(msgs[num_msg - 1]));

  scratch_buffers_reclaim_marked(marker);
  return result ^ s->comp;
}

static void
fop_cmp_free(FilterExprNode *s)
{
  FilterCmp *self = (FilterCmp *) s;

  log_template_unref(self->left);
  log_template_unref(self->right);
  g_free((gchar *) self->super.type);
}

FilterExprNode *
fop_cmp_clone(FilterExprNode *s)
{
  FilterCmp *self = (FilterCmp *) s;

  FilterCmp *cloned_self = g_new0(FilterCmp, 1);
  filter_expr_node_init_instance(&cloned_self->super);

  cloned_self->super.eval = fop_cmp_eval;
  cloned_self->super.free_fn = fop_cmp_free;
  cloned_self->super.clone = fop_cmp_clone;
  cloned_self->left = log_template_ref(self->left);
  cloned_self->right = log_template_ref(self->right);
  cloned_self->compare_mode = self->compare_mode;
  cloned_self->super.type = g_strdup(self->super.type);

  return &cloned_self->super;
}

FilterExprNode *
fop_cmp_new(LogTemplate *left, LogTemplate *right, const gchar *type, gint compare_mode)
{
  FilterCmp *self = g_new0(FilterCmp, 1);

  filter_expr_node_init_instance(&self->super);
  self->super.type = g_strdup(type);
  self->compare_mode = compare_mode;

  if (self->compare_mode & FCMP_TYPE_AWARE && cfg_is_config_version_older(left->cfg, VERSION_VALUE_3_8))
    {
      msg_warning("WARNING: due to a bug in versions before " VERSION_3_8
                  "numeric comparison operators like '!=' in filter "
                  "expressions were evaluated as string operators. This is fixed in " VERSION_3_8 ". "
                  "As we are operating in compatibility mode, syslog-ng will exhibit the buggy "
                  "behaviour as previous versions until you bump the @version value in your "
                  "configuration file");
      self->compare_mode &= ~FCMP_TYPE_AWARE;
      self->compare_mode |= FCMP_STRING_BASED;
    }

  g_assert((self->compare_mode & FCMP_MODE_MASK) != 0);
  self->super.eval = fop_cmp_eval;
  self->super.free_fn = fop_cmp_free;
  self->super.clone = fop_cmp_clone;
  self->left = left;
  self->right = right;

  return &self->super;
}
