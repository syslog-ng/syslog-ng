/*
 * Copyright (c) 2002-2019 Balabit
 * Copyright (c) 2019 Laszlo Budai <laszlo.budai@outlook.com>
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

#include "consecutive_ack_record_container.h"
#include "ringbuffer.h"

typedef struct _StaticConsecutiveAckRecordContainer
{
  ConsecutiveAckRecordContainer super;
  RingBuffer ack_records;
  ConsecutiveAckRecord *pending;
} StaticConsecutiveAckRecordContainer;

static gboolean
_is_empty(const ConsecutiveAckRecordContainer *s)
{
  StaticConsecutiveAckRecordContainer *self = (StaticConsecutiveAckRecordContainer *)s;

  return ring_buffer_is_empty(&self->ack_records);
}

static ConsecutiveAckRecord *
_request_pending(ConsecutiveAckRecordContainer *s)
{
  StaticConsecutiveAckRecordContainer *self = (StaticConsecutiveAckRecordContainer *)s;

  if (self->pending)
    return self->pending;

  self->pending = ring_buffer_tail(&self->ack_records);

  return self->pending;
}

static void
_store_pending(ConsecutiveAckRecordContainer *s)
{
  StaticConsecutiveAckRecordContainer *self = (StaticConsecutiveAckRecordContainer *)s;
  g_assert(ring_buffer_push(&self->ack_records) == self->pending);
  self->pending = NULL;
}

static void
_drop(ConsecutiveAckRecordContainer *s, gsize n)
{
  StaticConsecutiveAckRecordContainer *self = (StaticConsecutiveAckRecordContainer *)s;
  int i;
  ConsecutiveAckRecord *ack_rec;

  for (i = 0; i < n; i++)
    {
      ack_rec = (ConsecutiveAckRecord *)ring_buffer_element_at(&self->ack_records, i);
      ack_rec->acked = FALSE;

      consecutive_ack_record_destroy(ack_rec);

      ack_rec->super.bookmark.save = NULL;
      ack_rec->super.bookmark.destroy = NULL;
    }
  ring_buffer_drop(&self->ack_records, n);
}

static ConsecutiveAckRecord *
_at(const ConsecutiveAckRecordContainer *s, gsize idx)
{
  StaticConsecutiveAckRecordContainer *self = (StaticConsecutiveAckRecordContainer *)s;

  return ring_buffer_element_at(&self->ack_records, idx);
}

static void
_free(ConsecutiveAckRecordContainer *s)
{
  StaticConsecutiveAckRecordContainer *self = (StaticConsecutiveAckRecordContainer *)s;

  guint32 count = ring_buffer_count(&self->ack_records);
  _drop(s, count);
  ring_buffer_free(&self->ack_records);

  g_free(self);
}

static gsize
_size(const ConsecutiveAckRecordContainer *s)
{
  StaticConsecutiveAckRecordContainer *self = (StaticConsecutiveAckRecordContainer *)s;

  return ring_buffer_count(&self->ack_records);
}

static inline gboolean
_ack_range_is_continuous(void *data)
{
  ConsecutiveAckRecord *ack_rec = (ConsecutiveAckRecord *)data;

  return ack_rec->acked;
}

static gsize
_get_continual_range_length(const ConsecutiveAckRecordContainer *s)
{
  StaticConsecutiveAckRecordContainer *self = (StaticConsecutiveAckRecordContainer *)s;

  return ring_buffer_get_continual_range_length(&self->ack_records, _ack_range_is_continuous);
}

ConsecutiveAckRecordContainer *
consecutive_ack_record_container_static_new(gsize size)
{
  StaticConsecutiveAckRecordContainer *self = g_new0(StaticConsecutiveAckRecordContainer, 1);

  self->super.is_empty = _is_empty;
  self->super.request_pending = _request_pending;
  self->super.store_pending = _store_pending;
  self->super.drop = _drop;
  self->super.at = _at;
  self->super.free_fn = _free;
  self->super.size = _size;
  self->super.get_continual_range_length = _get_continual_range_length;

  ring_buffer_alloc(&self->ack_records, sizeof(ConsecutiveAckRecord), size);

  return &self->super;
}

