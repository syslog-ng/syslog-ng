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

#ifndef CONSECUTIVE_ACK_RECORD_CONTAINER_H_INCLUDED
#define CONSECUTIVE_ACK_RECORD_CONTAINER_H_INCLUDED

#include "ack_tracker.h"

typedef struct _ConsecutiveAckRecord
{
  AckRecord super;
  gboolean acked;
} ConsecutiveAckRecord;

typedef struct _ConsecutiveAckRecordContainer ConsecutiveAckRecordContainer;

struct _ConsecutiveAckRecordContainer
{
  gboolean (*is_empty)(const ConsecutiveAckRecordContainer *s);
  ConsecutiveAckRecord *(*request_pending)(ConsecutiveAckRecordContainer *s);
  void (*store_pending)(ConsecutiveAckRecordContainer *s);
  void (*drop)(ConsecutiveAckRecordContainer *s, gsize n);
  ConsecutiveAckRecord *(*at)(const ConsecutiveAckRecordContainer *s, gsize idx);
  void (*free_fn)(ConsecutiveAckRecordContainer *s);
  gsize (*size)(const ConsecutiveAckRecordContainer *s);
  gsize (*get_continual_range_length)(const ConsecutiveAckRecordContainer *s);
};

static inline void
consecutive_ack_record_destroy(ConsecutiveAckRecord *self)
{
  bookmark_destroy(&self->super.bookmark);
}

ConsecutiveAckRecordContainer *consecutive_ack_record_container_static_new(gsize size);
ConsecutiveAckRecordContainer *consecutive_ack_record_container_dynamic_new(void);

static inline gboolean
consecutive_ack_record_container_is_empty(ConsecutiveAckRecordContainer *s)
{
  return s->is_empty(s);
}

static inline ConsecutiveAckRecord *
consecutive_ack_record_container_request_pending(ConsecutiveAckRecordContainer *s)
{
  return s->request_pending(s);
}

static inline void
consecutive_ack_record_container_store_pending(ConsecutiveAckRecordContainer *s)
{
  s->store_pending(s);
}

static inline gsize
consecutive_ack_record_container_size(const ConsecutiveAckRecordContainer *s)
{
  return s->size(s);
}

static inline void
consecutive_ack_record_container_drop(ConsecutiveAckRecordContainer *s, gsize n)
{
  g_assert(n <= consecutive_ack_record_container_size(s));
  s->drop(s, n);
}

static inline ConsecutiveAckRecord *
consecutive_ack_record_container_at(const ConsecutiveAckRecordContainer *s, gsize idx)
{
  return s->at(s, idx);
}

static inline void
consecutive_ack_record_container_free(ConsecutiveAckRecordContainer *s)
{
  s->free_fn(s);
}

static inline gsize
consecutive_ack_record_container_get_continual_range_length(const ConsecutiveAckRecordContainer *s)
{
  return s->get_continual_range_length(s);
}

#endif

