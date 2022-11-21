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
  MLL_EXTRACTED    = 0x0001,
  MLL_WAITING      = 0x0002,
  MLL_CONSUME_LINE = 0x0010,
  MLL_REWIND_LINE  = 0x0020,
};

#define MLL_CONSUME_PARTIAL_AMOUNT_SHIFT     8
#define MLL_CONSUME_PARTIAL_AMOUNT_MASK      ~0xFF
#define MLL_CONSUME_PARTIALLY(drop_length) (MLL_CONSUME_LINE | ((drop_length) << MLL_CONSUME_PARTIAL_AMOUNT_SHIFT))

typedef struct _MultiLineLogic MultiLineLogic;
struct _MultiLineLogic
{
  gint (*accumulate_line)(MultiLineLogic *self,
                          const guchar *msg,
                          gsize msg_len,
                          gssize consumed_len);
  void (*free_fn)(MultiLineLogic *s);
};

void multi_line_logic_init_instance(MultiLineLogic *self);
void multi_line_logic_free_method(MultiLineLogic *s);

/*
 * multi_line_logic_accumulate_line():
 *
 * Accumulate a multi-line message into an internal buffer.
 *    msg
 *      points to the beginning of the line _repeatedly_, e.g. as
 *      long as we return the we couldn't extract a message.
 *
 *    msg_len
 *      This is getting longer and longer as lines get accumulated
 *      in the message.
 *
 *    consumed_len
 *      Is the length of the message starting with "msg" that was already
 *      consumed by this function.  In practice this points to the EOL
 *      character of the last consumed line.
 */
static inline gint
multi_line_logic_accumulate_line(MultiLineLogic *self,
                                 const guchar *msg,
                                 gsize msg_len,
                                 gssize consumed_len)
{
  return self->accumulate_line(self, msg, msg_len, consumed_len);
}

static inline void
multi_line_logic_free(MultiLineLogic *self)
{
  self->free_fn(self);
}



#endif
