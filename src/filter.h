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

#ifndef FILTER_H_INCLUDED
#define FILTER_H_INCLUDED

#include "syslog-ng.h"
#include "logmsg.h"

struct _LogFilterRule;
struct _GlobalConfig;

typedef struct _FilterExprNode
{
  gboolean comp;
  gboolean (*eval)(struct _FilterExprNode *self, LogMessage *msg);
  void (*free_fn)(struct _FilterExprNode *self);
} FilterExprNode;

#define RE_MAX_MATCHES 10
extern gchar *re_matches[RE_MAX_MATCHES];


FilterExprNode *fop_or_new(FilterExprNode *e1, FilterExprNode *e2);
FilterExprNode *fop_and_new(FilterExprNode *e1, FilterExprNode *e2);

FilterExprNode *filter_facility_new(guint32 facilities);
FilterExprNode *filter_level_new(guint32 levels);
FilterExprNode *filter_prog_new(gchar *prog);
FilterExprNode *filter_host_new(gchar *host);
FilterExprNode *filter_match_new(gchar *re);
FilterExprNode *filter_call_new(gchar *rule, struct _GlobalConfig *cfg);

static inline gboolean
filter_expr_eval(FilterExprNode *self, LogMessage *msg)
{
  return self->eval(self, msg);
}

static inline void
filter_expr_free(FilterExprNode *self)
{
  if (self->free_fn)
    self->free_fn(self);
  else
    g_free(self);
}

typedef struct _LogFilterRule
{
  gint ref_cnt;
  GString *name;
  FilterExprNode *root;
} LogFilterRule;

LogFilterRule *log_filter_ref(LogFilterRule *self);
void log_filter_unref(LogFilterRule *self);

LogFilterRule *log_filter_rule_new(gchar *name, struct _FilterExprNode *expr);

#endif
