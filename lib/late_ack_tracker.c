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

#include "late_ack_tracker.h"
#include "bookmark.h"
#include "ringbuffer.h"
#include "syslog-ng.h"

typedef struct _LateAckRecord
{
  AckRecord super;
  gboolean acked;
  Bookmark bookmark;
} LateAckRecord;

typedef struct LateAckTracker
{
  AckTracker super;
  LateAckRecord *pending_ack_record;
  struct
  {
    GList *head;
    GList *tail;
    gsize size;
    GStaticMutex mutex;
  } ack_records;
  AckTrackerOnAllAcked on_all_acked;
} LateAckTracker;

static inline void
late_ack_record_destroy(LateAckRecord *self)
{
  if (self->bookmark.destroy)
    self->bookmark.destroy(&(self->bookmark));
}

void
late_ack_tracker_lock(AckTracker *s)
{
  LateAckTracker *self = (LateAckTracker *)s;
  g_static_mutex_lock(&self->ack_records.mutex);
}

void
late_ack_tracker_unlock(AckTracker *s)
{
  LateAckTracker *self = (LateAckTracker *)s;
  g_static_mutex_unlock(&self->ack_records.mutex);
}

void
late_ack_tracker_set_on_all_acked(AckTracker *s, AckTrackerOnAllAckedFunc func, gpointer user_data,
                                  GDestroyNotify user_data_free_fn)
{
  LateAckTracker *self = (LateAckTracker *)s;
  if (self->on_all_acked.user_data && self->on_all_acked.user_data_free_fn)
    {
      self->on_all_acked.user_data_free_fn(self->on_all_acked.user_data);
    }

  self->on_all_acked = (AckTrackerOnAllAcked)
  {
    .func = func,
    .user_data = user_data,
    .user_data_free_fn = user_data_free_fn
  };
}

static inline void
late_ack_tracker_on_all_acked_call(AckTracker *s)
{
  LateAckTracker *self = (LateAckTracker *)s;
  AckTrackerOnAllAcked *handler = &self->on_all_acked;
  if (handler->func)
    {
      handler->func(handler->user_data);
    }
}

static inline gboolean
_ack_range_is_continuous(void *data)
{
  LateAckRecord *ack_rec = (LateAckRecord *)data;

  return ack_rec->acked;
}

static void
late_ack_tracker_track_msg(AckTracker *s, LogMessage *msg)
{
  LateAckTracker *self = (LateAckTracker *)s;
  LogSource *source = self->super.source;

  g_assert(self->pending_ack_record != NULL);

  log_pipe_ref((LogPipe *)source);

  msg->ack_record = (AckRecord *)self->pending_ack_record;

  late_ack_tracker_lock(s);
  {
    if (self->ack_records.head == NULL)
      {
        self->ack_records.head = g_list_append(self->ack_records.head, self->pending_ack_record);
        self->ack_records.tail = self->ack_records.head;
      }
    else
      {
        g_assert(g_list_append(self->ack_records.tail, self->pending_ack_record) == self->ack_records.tail);
        self->ack_records.tail = self->ack_records.tail->next;
      }
    self->ack_records.size++;
  }
  late_ack_tracker_unlock(s);

  self->pending_ack_record = NULL;
}

static void
late_ack_tracker_manage_msg_ack(AckTracker *s, LogMessage *msg, AckType ack_type)
{
  LateAckTracker *self = (LateAckTracker *)s;
  LateAckRecord *ack_rec = (LateAckRecord *)msg->ack_record;
  LateAckRecord *last_in_range = NULL;
  guint32 ack_range_length = 0;

  ack_rec->acked = TRUE;

  if (ack_type == AT_SUSPENDED)
    log_source_flow_control_suspend(self->super.source);

  late_ack_tracker_lock(s);
  {
    GList *it = NULL;
    GList *last = NULL;
    for (it = self->ack_records.head; it != NULL && _ack_range_is_continuous(it->data); it = it->next)
      {
        ++ack_range_length;
        last = it;
      }
    if (ack_range_length > 0 && it == NULL) //all acked...
      it = self->ack_records.head;
    if (ack_range_length > 0)
      {
        last_in_range = (LateAckRecord *)last->data;
        if (ack_type != AT_ABORTED)
          {
            Bookmark *bookmark = &(last_in_range->bookmark);
            bookmark->save(bookmark);
          }
        GList *delete_head = self->ack_records.head;
        self->ack_records.head = last->next;
        self->ack_records.size -= ack_range_length;
        last->next = NULL;
        g_list_free_full(delete_head, g_free);

        if (ack_type == AT_SUSPENDED)
          log_source_flow_control_adjust_when_suspended(self->super.source, ack_range_length);
        else
          log_source_flow_control_adjust(self->super.source, ack_range_length);

        if (!self->ack_records.head || self->ack_records.size == 0)
          late_ack_tracker_on_all_acked_call(s);
      }
  }
  late_ack_tracker_unlock(s);

  log_msg_unref(msg);
  log_pipe_unref((LogPipe *)self->super.source);
}

gboolean
late_ack_tracker_is_empty(AckTracker *s)
{
  LateAckTracker *self = (LateAckTracker *)s;
  return !self->ack_records.head || self->ack_records.size == 0;
}

static LateAckRecord *
_alloc_ack_record(void)
{
  LateAckRecord *record = g_new0(LateAckRecord, 1);

  return record;
}

static Bookmark *
late_ack_tracker_request_bookmark(AckTracker *s)
{
  LateAckTracker *self = (LateAckTracker *)s;

  if (!self->pending_ack_record)
    self->pending_ack_record = _alloc_ack_record();

  if (self->pending_ack_record)
    {
      self->pending_ack_record->bookmark.persist_state = s->source->super.cfg->state;

      self->pending_ack_record->super.tracker = (AckTracker *)self;

      return &(self->pending_ack_record->bookmark);
    }

  return NULL;
}

static void
late_ack_tracker_free(AckTracker *s)
{
  LateAckTracker *self = (LateAckTracker *)s;
  AckTrackerOnAllAcked *handler = &self->on_all_acked;

  if (handler->user_data_free_fn && handler->user_data)
    {
      handler->user_data_free_fn(handler->user_data);
    }

  g_static_mutex_free(&self->ack_records.mutex);

  if (self->pending_ack_record)
    g_free(self->pending_ack_record);

  g_list_free_full(self->ack_records.head, g_free);
  self->ack_records.head = NULL;

  g_free(self);
}

static void
_setup_callbacks(LateAckTracker *self)
{
  self->super.request_bookmark = late_ack_tracker_request_bookmark;
  self->super.track_msg = late_ack_tracker_track_msg;
  self->super.manage_msg_ack = late_ack_tracker_manage_msg_ack;
  self->super.free_fn = late_ack_tracker_free;
}

static void
late_ack_tracker_init_instance(LateAckTracker *self, LogSource *source)
{
  self->super.source = source;
  source->ack_tracker = (AckTracker *)self;
  g_static_mutex_init(&self->ack_records.mutex);
  _setup_callbacks(self);
}

AckTracker *
late_ack_tracker_new(LogSource *source)
{
  LateAckTracker *self = g_new0(LateAckTracker, 1);

  late_ack_tracker_init_instance(self, source);

  return (AckTracker *)self;
}

