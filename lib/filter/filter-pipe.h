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

#ifndef FILTER_PIPE_H_INCLUDED
#define FILTER_PIPE_H_INCLUDED

#include "filter/filter-expr.h"
#include "filter/optimizer/filter-expr-optimizer.h"
#include "logpipe.h"

/* convert a filter expression into a drop/accept LogPipe */

/*
 * This class encapsulates a LogPipe that either drops/allows a LogMessage
 * to go through.
 */
typedef struct _LogFilterPipe
{
  LogPipe super;
  FilterExprNode *expr;
  gchar *name;
  StatsCounterItem *matched;
  StatsCounterItem *not_matched;
  GPtrArray *optimizers;
} LogFilterPipe;

LogPipe *log_filter_pipe_new(FilterExprNode *expr, GlobalConfig *cfg);

void log_filter_pipe_register_optimizer(LogFilterPipe *self, FilterExprOptimizer *optimizer);

#endif
