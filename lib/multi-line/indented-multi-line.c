/*
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Gergely Nagy <algernon@balabit.hu>
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
 */
#include "multi-line-logic.h"

static inline gboolean
_is_line_a_continuation_line(guchar first_char)
{
  return (first_char == ' ' || first_char == '\t');
}

static gint
_accumulate_line(MultiLineLogic *s,
                 const guchar *msg,
                 gsize msg_len,
                 const guchar *segment,
                 gsize segment_len)
{
  /* let's check if the current line is a continuation line or not */
  if (msg_len > 0 && segment_len > 0)
    {
      guchar first_character_of_the_current_line = *segment;

      if (_is_line_a_continuation_line(first_character_of_the_current_line))
        {
          return MLL_CONSUME_SEGMENT | MLL_WAITING;
        }
      else
        {
          return MLL_REWIND_SEGMENT | MLL_EXTRACTED;
        }
    }

  return MLL_CONSUME_SEGMENT | MLL_WAITING;
}

MultiLineLogic *
indented_multi_line_new(void)
{
  MultiLineLogic *self = g_new0(MultiLineLogic, 1);

  multi_line_logic_init_instance(self);
  self->accumulate_line = _accumulate_line;
  return self;
}
