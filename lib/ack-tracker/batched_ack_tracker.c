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

typedef struct _BatchedAckTracker
{
  AckTracker super;
  guint timeout;
  guint batch_size;
} BatchedAckTracker;

static Bookmark *
_request_bookmark(AckTracker *s)
{
  // TODO -> create pending ack record
  return NULL;
}

static void
_track_msg(AckTracker *s, LogMessage *msg)
{
  // TODO -> assign ack record to msg, pending: null
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
_init(AckTracker *s, guint timeout, guint batch_size)
{
  BatchedAckTracker *self = (BatchedAckTracker *) s;

  _setup_callbacks(s);

  self->timeout = timeout;
  self->batch_size = batch_size;
}

AckTracker *
batched_ack_tracker_new(LogSource *source, guint timeout, guint batch_size)
{
  BatchedAckTracker *self = g_new0(BatchedAckTracker, 1);

  _init(&self->super, timeout, batch_size);

  return &self->super;
}
