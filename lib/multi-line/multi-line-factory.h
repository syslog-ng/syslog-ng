/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balazs Scheidler <bazsi@balabit.hu>
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

#ifndef MULTI_LINE_MULTI_LINE_FACTORY_H_INCLUDED
#define MULTI_LINE_MULTI_LINE_FACTORY_H_INCLUDED

#include "multi-line/regexp-multi-line.h"

enum
{
  MLM_NONE,
  MLM_INDENTED,
  MLM_REGEXP_PREFIX_GARBAGE,
  MLM_REGEXP_PREFIX_SUFFIX,
  MLM_SMART,
};

typedef struct _MultiLineOptions
{
  gint mode;
  union
  {
    struct
    {
      MultiLinePattern *prefix;
      MultiLinePattern *garbage;
    } regexp;
  };
} MultiLineOptions;

MultiLineLogic *multi_line_factory_construct(const MultiLineOptions *options);

gboolean multi_line_options_set_mode(MultiLineOptions *options, const gchar *mode);
gboolean multi_line_options_set_prefix(MultiLineOptions *options,
                                       const gchar *prefix_regexp, GError **error);
gboolean multi_line_options_set_garbage(MultiLineOptions *options,
                                        const gchar *garbage_regexp, GError **error);

gboolean multi_line_options_validate(MultiLineOptions *options);
void multi_line_options_copy(MultiLineOptions *dest, MultiLineOptions *source);
void multi_line_options_defaults(MultiLineOptions *options);
gboolean multi_line_options_init(MultiLineOptions *options);
void multi_line_options_destroy(MultiLineOptions *options);

void multi_line_global_init(void);
void multi_line_global_deinit(void);


#endif
