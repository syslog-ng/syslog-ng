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

#ifndef MULTI_LINE_LOGIC_H_INCLUDED
#define MULTI_LINE_LOGIC_H_INCLUDED

#include "syslog-ng.h"

enum
{
  MLL_EXTRACTED       = 0x0001,
  MLL_WAITING         = 0x0002,
  MLL_CONSUME_SEGMENT = 0x0010,
  MLL_REWIND_SEGMENT  = 0x0020,
};

#define MLL_CONSUME_PARTIAL_AMOUNT_SHIFT     8
#define MLL_CONSUME_PARTIAL_AMOUNT_MASK      ~0xFF
#define MLL_CONSUME_PARTIALLY(drop_length) (MLL_CONSUME_SEGMENT | ((drop_length) << MLL_CONSUME_PARTIAL_AMOUNT_SHIFT))

typedef struct _MultiLineLogic MultiLineLogic;
struct _MultiLineLogic
{
  gint (*accumulate_line)(MultiLineLogic *self,
                          const guchar *consumed,
                          gsize consumed_len,
                          const guchar *segment,
                          gsize segment_len);
  void (*free_fn)(MultiLineLogic *s);
};

void multi_line_logic_init_instance(MultiLineLogic *self);
void multi_line_logic_free_method(MultiLineLogic *s);

/*
 * multi_line_logic_accumulate_line():
 *
 * Accumulate a multi-line message into an internal buffer.
 *    consumed
 *      points to the buffer containing our consumed data so far
 *
 *    consumed_len
 *      The number of bytes in @consumed
 *
 *    segment
 *      new data to be considered part of consumed
 *
 *    segment_len
 *      The number of bytes in @segment
 *
 * The accumulator should return a set of bitfields that indicate what to do
 * with the data presented:
 *
 * What we want to do with the new data
 *     MLL_CONSUME_SEGMENT      -- add the new segment to data consumed
 *     MLL_CONSUME_PARTIALLY(n) -- add the new segment to the data consumed,
 *                                 but dropping (n) characters from the end
 *     MLL_REWIND_SEGMENT       -- the new data is NOT part of the consumed
 *                                 data so far, and should be considered
 *                                 part of the next line.
 *
 * What we want to perform once the accumulation is finished:
 *     MLL_EXTRACTED            -- the accumulation is finished, return the
 *                                 consumed data as a complete line for higher
 *                                 layers.
 *     MLL_WAITING              -- we still need more data
 *
 */
static inline gint
multi_line_logic_accumulate_line(MultiLineLogic *self,
                                 const guchar *msg,
                                 gsize msg_len,
                                 const guchar *segment,
                                 gsize segment_len)
{
  return self->accumulate_line(self, msg, msg_len, segment, segment_len);
}

static inline void
multi_line_logic_free(MultiLineLogic *self)
{
  self->free_fn(self);
}



#endif
