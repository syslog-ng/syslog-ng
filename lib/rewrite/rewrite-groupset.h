/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Viktor Tusa <viktor.tusa@balabit.com>
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

#include "rewrite/rewrite-expr.h"
#include "value-pairs/value-pairs.h"

typedef struct _LogRewriteGroupSet
{
  LogRewrite super;
  ValuePairs *query;
  LogTemplate *replacement;
  VPForeachFunc vp_func;
} LogRewriteGroupSet;

LogRewrite *log_rewrite_groupset_new(LogTemplate *template, GlobalConfig *cfg);
LogRewrite *log_rewrite_groupunset_new(GlobalConfig *cfg);
void log_rewrite_groupset_add_fields(LogRewrite *rewrite, GList *fields);
