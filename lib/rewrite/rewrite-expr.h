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

#ifndef REWRITE_H_INCLUDED
#define REWRITE_H_INCLUDED

#include "logpipe.h"
#include "filter/filter-expr.h"

typedef struct _LogRewrite LogRewrite;

struct _LogRewrite
{
  LogPipe super;
  NVHandle value_handle;
  FilterExprNode *condition;
  void (*process)(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options);
  gchar *name;
};

/* LogRewrite, abstract class */
gboolean log_rewrite_init_method(LogPipe *s);
void log_rewrite_clone_method(LogRewrite *dst, const LogRewrite *src);

void log_rewrite_set_condition(LogRewrite *s, FilterExprNode *condition);
void log_rewrite_init_instance(LogRewrite *self, GlobalConfig *cfg);
void log_rewrite_free_method(LogPipe *self);

#endif
