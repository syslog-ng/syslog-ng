/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 2014 Laszlo Budai
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

#ifndef RINGBUFFER_H_INCLUDED
#define RINGBUFFER_H_INCLUDED

#include <glib.h>

typedef struct _RingBuffer
{
  gpointer buffer;
  guint32 head;
  guint32 tail;
  guint32 count;
  guint32 capacity;
  guint32 element_size;
} RingBuffer;

typedef gboolean(*RingBufferIsContinuousPredicate)(gpointer element);

void ring_buffer_init(RingBuffer *self);
void ring_buffer_alloc(RingBuffer *self, guint32 size_of_element, guint32 capacity);
gboolean ring_buffer_is_allocated(RingBuffer *self);
void ring_buffer_free(RingBuffer *self);

gboolean ring_buffer_is_full(RingBuffer *self);
gboolean ring_buffer_is_empty(RingBuffer *self);

gpointer ring_buffer_push(RingBuffer *self);
gpointer ring_buffer_pop(RingBuffer *self);
gpointer ring_buffer_tail(RingBuffer *self);

gboolean ring_buffer_drop(RingBuffer *self, guint32 n);
guint32 ring_buffer_capacity(RingBuffer *self);
guint32 ring_buffer_count(RingBuffer *self);

gpointer ring_buffer_element_at(RingBuffer *self, guint32 idx);
guint32 ring_buffer_get_continual_range_length(RingBuffer *self, RingBufferIsContinuousPredicate pred);

#endif
