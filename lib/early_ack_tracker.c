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

#include "ack_tracker.h"
#include "bookmark.h"
#include "syslog-ng.h"

typedef struct _EarlyAckRecord
{
  AckRecord super;
  void *padding;
  /* bookmark contains a binary container which has to be aligned */
  Bookmark bookmark;
} EarlyAckRecord;

typedef struct _EarlyAckTracker
{
  AckTracker super;

  EarlyAckRecord ack_record_storage;
} EarlyAckTracker;

static Bookmark *
early_ack_tracker_request_bookmark(AckTracker *s)
{
  EarlyAckTracker *self = (EarlyAckTracker *)s;
  return &(self->ack_record_storage.bookmark);
}

static void
early_ack_tracker_track_msg(AckTracker *s, LogMessage *msg)
{
  EarlyAckTracker *self = (EarlyAckTracker *)s;
  log_pipe_ref((LogPipe *)self->super.source);
  msg->ack_record = (AckRecord *)(&self->ack_record_storage);
}

static void
early_ack_tracker_manage_msg_ack(AckTracker *s, LogMessage *msg, AckType ack_type)
{
  EarlyAckTracker *self = (EarlyAckTracker *)s;

  log_source_flow_control_adjust(self->super.source, 1);

  if (ack_type == AT_SUSPENDED)
    log_source_flow_control_suspend(self->super.source);

  log_msg_unref(msg);
  log_pipe_unref((LogPipe *)self->super.source);
}

static void
early_ack_tracker_free(AckTracker *s)
{
  EarlyAckTracker *self = (EarlyAckTracker *)s;

  g_free(self);
}

static void
_setup_callbacks(EarlyAckTracker *self)
{
  self->super.request_bookmark = early_ack_tracker_request_bookmark;
  self->super.track_msg = early_ack_tracker_track_msg;
  self->super.manage_msg_ack = early_ack_tracker_manage_msg_ack;
  self->super.free_fn = early_ack_tracker_free;
}

static void
early_ack_tracker_init_instance(EarlyAckTracker *self, LogSource *source)
{
  self->super.late = FALSE;
  self->super.source = source;
  source->ack_tracker = (AckTracker *)self;
  self->ack_record_storage.super.tracker = (AckTracker *)self;
  _setup_callbacks(self);
}

AckTracker *
early_ack_tracker_new(LogSource *source)
{
  EarlyAckTracker *self = (EarlyAckTracker *)g_new0(EarlyAckTracker, 1);

  early_ack_tracker_init_instance(self, source);

  return (AckTracker *)self;
}

