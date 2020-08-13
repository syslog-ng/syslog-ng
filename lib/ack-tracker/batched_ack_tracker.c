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
  Bookmark bookmark;
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
} BatchedAckTracker;

static Bookmark *
_request_bookmark(AckTracker *s)
{
  BatchedAckTracker *self = (BatchedAckTracker *) s;
  if (!self->pending_ack_record)
    self->pending_ack_record = g_new0(BatchedAckRecord, 1);

  return &(self->pending_ack_record->bookmark);
}

static void
_assign_pending_ack_record_to_msg(BatchedAckTracker *self, LogMessage *msg)
{
  LogSource *source = self->super.source;
  log_pipe_ref((LogPipe *) source);
  self->pending_ack_record->bookmark.persist_state = source->super.cfg->state;
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

static void
_manage_msg_ack(AckTracker *s, LogMessage *msg, AckType ack_type)
{
  // TODO -> collect ack record, when batch is full-> ack all, note: need a lock
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
