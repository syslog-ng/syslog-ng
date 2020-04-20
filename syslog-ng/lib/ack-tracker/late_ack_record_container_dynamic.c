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

typedef struct _DynamicLateAckRecordContainer
{
  LateAckRecordContainer super;
  GList *head;
  GList *tail;
  gsize size;
  LateAckRecord *pending;
} DynamicLateAckRecordContainer;

static gboolean
_is_empty(const LateAckRecordContainer *s)
{
  DynamicLateAckRecordContainer *self = (DynamicLateAckRecordContainer *) s;

  return (!self->head || self->size == 0);
}

static LateAckRecord *
_request_pending(LateAckRecordContainer *s)
{
  DynamicLateAckRecordContainer *self = (DynamicLateAckRecordContainer *)s;
  if (self->pending)
    return self->pending;

  self->pending = g_new0(LateAckRecord, 1);

  return self->pending;
}

static void
_store_pending(LateAckRecordContainer *s)
{
  DynamicLateAckRecordContainer *self = (DynamicLateAckRecordContainer *) s;

  if (self->head == NULL)
    {
      self->head = g_list_append(self->head, self->pending);
      self->tail = self->head;
    }
  else
    {
      g_assert(g_list_append(self->tail, self->pending) == self->tail);
      self->tail = self->tail->next;
    }

  self->size++;
  self->pending = NULL;
}

static void
_free_and_destroy_ack_record(gpointer data)
{
  LateAckRecord *rec = (LateAckRecord *)data;
  late_ack_record_destroy(rec);
  g_free(rec);
}

static void
_drop(LateAckRecordContainer *s, gsize n)
{
  DynamicLateAckRecordContainer *self = (DynamicLateAckRecordContainer *) s;

  if (n == self->size)
    {
      g_list_free_full(self->head, _free_and_destroy_ack_record);
      self->head = self->tail = NULL;
      self->size = 0;
      return;
    }

  GList *delete_head = self->head;
  GList *last = g_list_nth(self->head, n-1);

  self->head = last->next;
  self->head->prev = NULL;
  self->size -= n;
  last->next = NULL;

  g_list_free_full(delete_head, _free_and_destroy_ack_record);
}

static LateAckRecord *
_at(const LateAckRecordContainer *s, gsize idx)
{
  DynamicLateAckRecordContainer *self = (DynamicLateAckRecordContainer *) s;

  return (LateAckRecord *) g_list_nth(self->head, idx)->data;
}

static void
_free(LateAckRecordContainer *s)
{
  DynamicLateAckRecordContainer *self = (DynamicLateAckRecordContainer *)s;

  if (self->pending)
    {
      late_ack_record_destroy(self->pending);
      g_free(self->pending);
      self->pending = NULL;
    }

  g_list_free_full(self->head, _free_and_destroy_ack_record);
  self->head = NULL;
  self->tail = NULL;
  g_free(self);
}

static gsize
_size(const LateAckRecordContainer *s)
{
  DynamicLateAckRecordContainer *self = (DynamicLateAckRecordContainer *)s;

  return self->size;
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
  DynamicLateAckRecordContainer *self = (DynamicLateAckRecordContainer *)s;
  guint32 ack_range_length = 0;

  for (GList *it = self->head;
       it != NULL && _ack_range_is_continuous(it->data);
       it = it->next)
    {
      ++ack_range_length;
    }

  return ack_range_length;
}

LateAckRecordContainer *
late_ack_record_container_dynamic_new(void)
{
  DynamicLateAckRecordContainer *self = g_new0(DynamicLateAckRecordContainer, 1);

  self->super.is_empty = _is_empty;
  self->super.request_pending = _request_pending;
  self->super.store_pending = _store_pending;
  self->super.drop = _drop;
  self->super.at = _at;
  self->super.free_fn = _free;
  self->super.size = _size;
  self->super.get_continual_range_length = _get_continual_range_length;

  return &self->super;
}

