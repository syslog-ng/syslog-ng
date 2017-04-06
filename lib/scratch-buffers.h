/*
 * Copyright (c) 2011-2013 Balabit
 * Copyright (c) 2011-2013 Gergely Nagy <algernon@balabit.hu>
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

#ifndef SCRATCH_BUFFERS_H_INCLUDED
#define SCRATCH_BUFFERS_H_INCLUDED 1

#include <glib.h>
#include "type-hinting.h"

/* Global API */

typedef struct
{
  GTrashStack *(*acquire_buffer)(void);
  void (*release_buffer)(GTrashStack *stack);
  void (*free_stack)(void);
} ScratchBufferStack;

static inline GTrashStack *
scratch_buffer_acquire(ScratchBufferStack *stack)
{
  return stack->acquire_buffer();
}

static inline void
scratch_buffer_release(ScratchBufferStack *stack, GTrashStack *buffer)
{
  stack->release_buffer(buffer);
}

void scratch_buffers_register(ScratchBufferStack *stack);
void scratch_buffers_init(void);
void scratch_buffers_free(void);

/* GStrings */

typedef struct
{
  GTrashStack stackp;
  GString s;
} SBGString;

extern ScratchBufferStack SBGStringStack;

#define sb_gstring_acquire() ((SBGString *)scratch_buffer_acquire(&SBGStringStack))
#define sb_gstring_release(b) (scratch_buffer_release(&SBGStringStack, (GTrashStack *)b))

#define sb_gstring_string(buffer) (&buffer->s)

/* Type-hinted GStrings */

typedef struct
{
  GTrashStack stackp;
  GString s;
  TypeHint type_hint;
} SBTHGString;

extern ScratchBufferStack SBTHGStringStack;

#define sb_th_gstring_acquire() ((SBTHGString *)scratch_buffer_acquire(&SBTHGStringStack))
#define sb_th_gstring_release(b) (scratch_buffer_release(&SBTHGStringStack, (GTrashStack *)b))

#define sb_th_gstring_string(buffer) (&buffer->s)

#endif
