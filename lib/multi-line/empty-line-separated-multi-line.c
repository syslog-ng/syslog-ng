/*
 * Copyright (c) 2025 One Identity
 * Copyright (c) 2025 Hofi <hofione@gmail.com>
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
 */

#include "empty-line-separated-multi-line.h"
#include "reloc.h"
#include "messages.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct _EmpytLineSeparatedMultiLine
{
  MultiLineLogic super;
  GMutex lock;
} EmpytLineSeparatedMultiLine;

static gint
_accumulate_line(MultiLineLogic *s,
                 const guchar *msg, gsize msg_len,
                 const guchar *segment, gsize segment_len)
{
  EmpytLineSeparatedMultiLine *self = (EmpytLineSeparatedMultiLine *) s;

  g_mutex_lock(&self->lock);
  gint result = MLL_WAITING | MLL_CONSUME_SEGMENT;
  if (segment_len == 0 || (segment_len == 1 && segment[0] == '\r'))
    result = MLL_EXTRACTED | MLL_CONSUME_SEGMENT;
  g_mutex_unlock(&self->lock);
  return result;
}

static void
_free(MultiLineLogic *s)
{
  EmpytLineSeparatedMultiLine *self = (EmpytLineSeparatedMultiLine *) s;
  g_mutex_clear(&self->lock);
  multi_line_logic_free_method(s);
}

MultiLineLogic *
empty_line_separated_multi_line_new(void)
{
  EmpytLineSeparatedMultiLine *self = g_new0(EmpytLineSeparatedMultiLine, 1);

  multi_line_logic_init_instance(&self->super);
  self->super.free_fn = _free;
  self->super.accumulate_line = _accumulate_line;
  self->super.keep_trailing_newline = TRUE;
  g_mutex_init(&self->lock);

  return &self->super;
}
