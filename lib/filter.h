/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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
  
#ifndef FILTER_H_INCLUDED
#define FILTER_H_INCLUDED

#include "syslog-ng.h"
#include "logmsg.h"
#include "messages.h"
#include "logprocess.h"
#include "logmatcher.h"
#include "cfg-parser.h"

struct _GlobalConfig;
typedef struct _FilterExprNode FilterExprNode;

struct _FilterExprNode
{
  guint32 ref_cnt;
  guint32 comp:1,   /* this not is negated */
          modify:1; /* this filter changes the log message */
  const gchar *type;
  void (*init)(FilterExprNode *self, GlobalConfig *cfg);
  gboolean (*eval)(FilterExprNode *self, LogMessage **msg, gint num_msg);
  void (*free_fn)(FilterExprNode *self);
};

gboolean filter_expr_eval(FilterExprNode *self, LogMessage *msg);
gboolean filter_expr_eval_with_context(FilterExprNode *self, LogMessage **msgs, gint num_msg);
void filter_expr_unref(FilterExprNode *self);

typedef struct _FilterRE
{
  FilterExprNode super;
  NVHandle value_handle;
  LogMatcher *matcher;
} FilterRE;

typedef struct _FilterMatch FilterMatch;

void filter_re_set_matcher(FilterRE *self, LogMatcher *matcher);
gboolean filter_re_set_regexp(FilterRE *self, gchar *re);
void filter_re_set_flags(FilterRE *self, gint flags);
void filter_tags_add(FilterExprNode *s, GList *tags);

/* various constructors */

FilterExprNode *fop_or_new(FilterExprNode *e1, FilterExprNode *e2);
FilterExprNode *fop_and_new(FilterExprNode *e1, FilterExprNode *e2);
FilterExprNode *fop_cmp_new(LogTemplate *left, LogTemplate *right, gint op);

FilterExprNode *filter_facility_new(guint32 facilities);
FilterExprNode *filter_level_new(guint32 levels);
FilterExprNode *filter_call_new(gchar *rule, struct _GlobalConfig *cfg);
FilterExprNode *filter_netmask_new(gchar *cidr);
FilterExprNode *filter_re_new(NVHandle value_handle);
FilterExprNode *filter_match_new(void);
FilterExprNode *filter_tags_new(GList *tags);

/* convert a filter expression into a drop/accept LogPipe */

/*
 * This class encapsulates a LogPipe that either drops/allows a LogMessage
 * to go through.
 */
typedef struct _LogFilterPipe
{
  LogProcessPipe super;
  FilterExprNode *expr;
  gchar *name;
} LogFilterPipe;


LogPipe *log_filter_pipe_new(FilterExprNode *expr, const gchar *name);

#endif
