/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Balazs Scheidler <bazsi@balabit.hu>
 * Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
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

#ifndef MULTI_LINE_REGEXP_MULTI_LINE_H_INCLUDED
#define MULTI_LINE_REGEXP_MULTI_LINE_H_INCLUDED

#include "multi-line/multi-line-logic.h"
#include "compat/pcre.h"


typedef struct _MultiLinePattern MultiLinePattern;
struct _MultiLinePattern
{
  gint ref_cnt;
  pcre *pattern;
  pcre_extra *extra;
};

MultiLinePattern *multi_line_pattern_compile(const gchar *regexp, GError **error);
MultiLinePattern *multi_line_pattern_ref(MultiLinePattern *self);
void multi_line_pattern_unref(MultiLinePattern *self);


typedef struct _RegexpMultiLine
{
  MultiLineLogic super;
  enum
  {
    RML_PREFIX_GARBAGE,
    RML_PREFIX_SUFFIX,
  } mode;
  MultiLinePattern *prefix;
  MultiLinePattern *garbage;
} RegexpMultiLine;

MultiLineLogic *regexp_multi_line_new(gint mode, MultiLinePattern *prefix, MultiLinePattern *garbage_or_suffix);

#endif
