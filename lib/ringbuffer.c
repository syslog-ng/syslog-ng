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

#include "ringbuffer.h"

void
ring_buffer_init(RingBuffer *self)
{
  self->head = 0;
  self->tail = 0;
  self->count = 0;
  self->capacity = 0;
  self->element_size = 0;
  self->buffer = NULL;
}

void
ring_buffer_alloc(RingBuffer *self, guint32 element_size, guint32 capacity)
{
  g_assert(capacity > 0);
  self->head = 0;
  self->tail = 0;
  self->count = 0;
  self->capacity = capacity;
  self->element_size = element_size;
  self->buffer = g_malloc0(element_size * self->capacity);
}

gboolean
ring_buffer_is_allocated(RingBuffer *self)
{
  return (self->buffer != NULL);
}

void
ring_buffer_free(RingBuffer *self)
{
  g_free(self->buffer);
  self->head = 0;
  self->tail = 0;
  self->count = 0;
  self->capacity = 0;
  self->element_size = 0;
  self->buffer = NULL;
}

gboolean
ring_buffer_is_full(RingBuffer *self)
{
  return self->capacity == self->count ? TRUE : FALSE;
}

gboolean
ring_buffer_is_empty(RingBuffer *self)
{
  return (self->count == 0) ? TRUE : FALSE;
}

gpointer
ring_buffer_push(RingBuffer *self)
{
  gpointer r = ring_buffer_tail(self);

  if (!r)
    return NULL;

  ++self->count;
  self->tail = (self->tail + 1) % self->capacity;

  return r;
}

gpointer
ring_buffer_tail (RingBuffer *self)
{
  g_assert(self->buffer != NULL);

  if (ring_buffer_is_full(self))
    return NULL;

  gpointer r = (guint8 *) (self->buffer) + self->tail * self->element_size;

  return r;
}

gpointer
ring_buffer_pop(RingBuffer *self)
{
  g_assert(self->buffer != NULL);

  if (ring_buffer_is_empty(self))
    return NULL;

  gpointer r = (guint8 *) (self->buffer) + self->head * self->element_size;

  --self->count;
  self->head = (self->head + 1) % self->capacity;

  return r;
}

gboolean
ring_buffer_drop(RingBuffer *self, guint32 n)
{
  g_assert(self->buffer != NULL);

  if (ring_buffer_count(self) < n)
    return FALSE;

  self->count -= n;
  self->head = (self->head + n) % self->capacity;

  return TRUE;
}

guint32
ring_buffer_capacity(RingBuffer *self)
{
  return self->buffer ? self->capacity : 0;
}

guint32
ring_buffer_count(RingBuffer *self)
{
  return self->count;
}

gpointer
ring_buffer_element_at(RingBuffer *self, guint32 idx)
{
  g_assert(self->buffer != NULL);

  if (idx >= self->count)
    return NULL;

  return (guint8 *) (self->buffer) + ((self->head + idx) % self->capacity) * self->element_size;
}

guint32
ring_buffer_get_continual_range_length(RingBuffer *self, RingBufferIsContinuousPredicate pred)
{
  guint32 r = 0, i;

  g_assert(self->buffer != NULL);

  for (i = 0; i < ring_buffer_count(self); i++)
    {
      if (!pred(ring_buffer_element_at(self, i)))
        {
          break;
        }
      ++r;
    }

  return r;
}
