/*
 * Copyright (c) 2020 One Identity
 * Copyright (c) 2020 Laszlo Budai
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

#include "batched_ack_tracker.h"
#include "bookmark.h"
#include "syslog-ng.h"
#include "logsource.h"

typedef struct _BatchedAckRecord
{
  AckRecord super;
} BatchedAckRecord;

typedef struct _OnBatchAckedFunctor
{
  BatchedAckTrackerOnBatchAcked func;
  gpointer user_data;
} OnBatchAckedFunctor;

typedef struct _BatchedAckTracker
{
  AckTracker super;
  guint timeout;
  guint batch_size;
  OnBatchAckedFunctor on_batch_acked;
  BatchedAckRecord *pending_ack_record;
  GMutex acked_records_lock;
  gulong acked_records_num;
  GList *acked_records;
} BatchedAckTracker;

static void
_ack_record_free(AckRecord *s)
{
  BatchedAckRecord *self = (BatchedAckRecord *) s;
  bookmark_destroy(&self->super.bookmark);

  g_free(self);
}

static void
_ack_batch(BatchedAckTracker *self, GList *ack_records)
{
  self->on_batch_acked.func(ack_records, self->on_batch_acked.user_data);
  g_list_free_full(ack_records, (GDestroyNotify) _ack_record_free);
}

static Bookmark *
_request_bookmark(AckTracker *s)
{
  BatchedAckTracker *self = (BatchedAckTracker *) s;
  if (!self->pending_ack_record)
    self->pending_ack_record = g_new0(BatchedAckRecord, 1);

  return &(self->pending_ack_record->super.bookmark);
}

static void
_assign_pending_ack_record_to_msg(BatchedAckTracker *self, LogMessage *msg)
{
  LogSource *source = self->super.source;
  log_pipe_ref((LogPipe *) source);
  self->pending_ack_record->super.bookmark.persist_state = source->super.cfg->state;
  self->pending_ack_record->super.tracker = &self->super;
  msg->ack_record = (AckRecord *) self->pending_ack_record;
}

static void
_track_msg(AckTracker *s, LogMessage *msg)
{
  BatchedAckTracker *self = (BatchedAckTracker *)s;
  _assign_pending_ack_record_to_msg(self, msg);
  self->pending_ack_record = NULL;
}

static GList *
_append_ack_record_to_batch(BatchedAckTracker *self, AckRecord *ack_record)
{
  GList *full_batch = NULL;
  g_mutex_lock(&self->acked_records_lock);
  {
    self->acked_records = g_list_prepend(self->acked_records, ack_record);
    ++self->acked_records_num;
    if (self->acked_records_num == self->batch_size)
      {
        full_batch = self->acked_records;
        self->acked_records = NULL;
        self->acked_records_num = 0;
      }
  }
  g_mutex_unlock(&self->acked_records_lock);

  return full_batch;
}

static void
_manage_msg_ack(AckTracker *s, LogMessage *msg, AckType ack_type)
{
  BatchedAckTracker *self = (BatchedAckTracker *) s;
  log_source_flow_control_adjust(self->super.source, 1);

  if (ack_type == AT_SUSPENDED)
    log_source_flow_control_suspend(self->super.source);

  if (ack_type != AT_ABORTED)
    {
      GList *full_batch = _append_ack_record_to_batch(self, msg->ack_record);
      if (full_batch)
        _ack_batch(self, full_batch);
    }
  else
    {
      _ack_record_free(msg->ack_record);
    }

  log_msg_unref(msg);
  log_pipe_unref((LogPipe *)self->super.source);
}

static void
_disable_bookmark_saving(AckTracker *self)
{
  // TODO -> ??
}

static void
_free(AckTracker *s)
{
  BatchedAckTracker *self = (BatchedAckTracker *) s;
  g_mutex_clear(&self->acked_records_lock);
  if (self->acked_records)
    _ack_batch(self, self->acked_records);

  if (self->pending_ack_record)
    _ack_record_free(&self->pending_ack_record->super);

  g_free(self);
}

static void
_setup_callbacks(AckTracker *s)
{
  s->request_bookmark = _request_bookmark;
  s->track_msg = _track_msg;
  s->manage_msg_ack = _manage_msg_ack;
  s->disable_bookmark_saving = _disable_bookmark_saving;
  s->free_fn = _free;
}

static void
_init(AckTracker *s, LogSource *source, guint timeout, guint batch_size,
      BatchedAckTrackerOnBatchAcked cb, gpointer user_data)
{
  BatchedAckTracker *self = (BatchedAckTracker *) s;

  _setup_callbacks(s);

  s->source = source;
  source->ack_tracker = s;

  self->timeout = timeout;
  self->batch_size = batch_size;
  self->pending_ack_record = NULL;

  self->on_batch_acked.func = cb;
  self->on_batch_acked.user_data = user_data;

  g_mutex_init(&self->acked_records_lock);
}

AckTracker *
batched_ack_tracker_new(LogSource *source, guint timeout, guint batch_size,
                        BatchedAckTrackerOnBatchAcked on_batch_acked, gpointer user_data)
{
  BatchedAckTracker *self = g_new0(BatchedAckTracker, 1);

  _init(&self->super, source, timeout, batch_size, on_batch_acked, user_data);
  /*
   * It is mandatory to process the batched ACKs
   * */
  g_assert(self->on_batch_acked.func != NULL);

  return &self->super;
}
