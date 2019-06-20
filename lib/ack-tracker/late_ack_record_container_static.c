/*
 * Copyright (c) 2002-2019 Balabit
 * Copyright (c) 2019 Laszlo Budai
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

#include "late_ack_record_container.h"
#include "ringbuffer.h"

typedef struct _StaticLateAckRecordContainer
{
  LateAckRecordContainer super;
  RingBuffer ack_records;
  LateAckRecord *pending;
} StaticLateAckRecordContainer;

static gboolean
_is_empty(const LateAckRecordContainer *s)
{
  StaticLateAckRecordContainer *self = (StaticLateAckRecordContainer *)s;

  return ring_buffer_is_empty(&self->ack_records);
}

static LateAckRecord *
_request_pending(LateAckRecordContainer *s)
{
  StaticLateAckRecordContainer *self = (StaticLateAckRecordContainer *)s;

  if (self->pending)
    return self->pending;

  self->pending = ring_buffer_tail(&self->ack_records);

  return self->pending;
}

static void
_store_pending(LateAckRecordContainer *s)
{
  StaticLateAckRecordContainer *self = (StaticLateAckRecordContainer *)s;
  g_assert(ring_buffer_push(&self->ack_records) == self->pending);
  self->pending = NULL;
}

static void
_drop(LateAckRecordContainer *s, gsize n)
{
  StaticLateAckRecordContainer *self = (StaticLateAckRecordContainer *)s;
  int i;
  LateAckRecord *ack_rec;

  g_assert(n <= ring_buffer_count(&self->ack_records));

  for (i = 0; i < n; i++)
    {
      ack_rec = (LateAckRecord *)ring_buffer_element_at(&self->ack_records, i);
      ack_rec->acked = FALSE;

      late_ack_record_destroy(ack_rec);

      ack_rec->bookmark.save = NULL;
      ack_rec->bookmark.destroy = NULL;
    }
  ring_buffer_drop(&self->ack_records, n);
}

static LateAckRecord *
_at(const LateAckRecordContainer *s, gsize idx)
{
  StaticLateAckRecordContainer *self = (StaticLateAckRecordContainer *)s;

  return ring_buffer_element_at(&self->ack_records, idx);
}

static void
_free(LateAckRecordContainer *s)
{
  StaticLateAckRecordContainer *self = (StaticLateAckRecordContainer *)s;

  guint32 count = ring_buffer_count(&self->ack_records);
  _drop(s, count);
  ring_buffer_free(&self->ack_records);

  g_free(self);
}

static gsize
_size(const LateAckRecordContainer *s)
{
  StaticLateAckRecordContainer *self = (StaticLateAckRecordContainer *)s;

  return ring_buffer_count(&self->ack_records);
}

static inline gboolean
_ack_range_is_continuous(void *data)
{
  LateAckRecord *ack_rec = (LateAckRecord *)data;

  return ack_rec->acked;
}

static gsize
_get_continual_range_length(const LateAckRecordContainer *s)
{
  StaticLateAckRecordContainer *self = (StaticLateAckRecordContainer *)s;

  return ring_buffer_get_continual_range_length(&self->ack_records, _ack_range_is_continuous);
}

LateAckRecordContainer *
late_ack_record_container_static_new(gsize size)
{
  StaticLateAckRecordContainer *self = g_new0(StaticLateAckRecordContainer, 1);

  self->super.is_empty = _is_empty;
  self->super.request_pending = _request_pending;
  self->super.store_pending = _store_pending;
  self->super.drop = _drop;
  self->super.at = _at;
  self->super.free_fn = _free;
  self->super.size = _size;
  self->super.get_continual_range_length = _get_continual_range_length;

  ring_buffer_alloc(&self->ack_records, sizeof(LateAckRecord), size);

  return &self->super;
}

