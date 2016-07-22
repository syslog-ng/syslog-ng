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

#ifndef FILTER_RE_H_INCLUDED
#define FILTER_RE_H_INCLUDED

#include "filter-expr.h"
#include "logmatcher.h"

typedef struct _FilterRE
{
  FilterExprNode super;
  NVHandle value_handle;
  LogMatcherOptions matcher_options;
  LogMatcher *matcher;
} FilterRE;

typedef struct _FilterMatch FilterMatch;

gboolean filter_re_compile_pattern(FilterRE *self, GlobalConfig *cfg, const gchar *re, GError **error);

FilterRE *filter_re_new(NVHandle value_handle);
FilterRE *filter_source_new(void);
FilterRE *filter_match_new(void);

#endif
