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

#include "instant_ack_tracker.h"
#include "bookmark.h"
#include "syslog-ng.h"

typedef struct _InstantAckRecord
{
  AckRecord super;
  void *padding;
  /* bookmark contains a binary container which has to be aligned */
} InstantAckRecord;

typedef struct _InstantAckTrackerBookmarkless
{
  AckTracker super;

  InstantAckRecord ack_record_storage;
} InstantAckTrackerBookmarkless;

static Bookmark *
_request_bookmark(AckTracker *s)
{
  InstantAckTrackerBookmarkless *self = (InstantAckTrackerBookmarkless *)s;
  return &(self->ack_record_storage.super.bookmark);
}

static void
_track_msg(AckTracker *s, LogMessage *msg)
{
  InstantAckTrackerBookmarkless *self = (InstantAckTrackerBookmarkless *)s;
  log_pipe_ref((LogPipe *)self->super.source);
  msg->ack_record = (AckRecord *)(&self->ack_record_storage);
}

static void
_manage_msg_ack(AckTracker *s, LogMessage *msg, AckType ack_type)
{
  InstantAckTrackerBookmarkless *self = (InstantAckTrackerBookmarkless *)s;

  log_source_flow_control_adjust(self->super.source, 1);

  if (ack_type == AT_SUSPENDED)
    log_source_flow_control_suspend(self->super.source);

  log_msg_unref(msg);
  log_pipe_unref((LogPipe *)self->super.source);
}

static void
_free(AckTracker *s)
{
  InstantAckTrackerBookmarkless *self = (InstantAckTrackerBookmarkless *)s;

  g_free(self);
}

static void
_setup_callbacks(InstantAckTrackerBookmarkless *self)
{
  self->super.request_bookmark = _request_bookmark;
  self->super.track_msg = _track_msg;
  self->super.manage_msg_ack = _manage_msg_ack;
  self->super.free_fn = _free;
}

static void
_init_instance(InstantAckTrackerBookmarkless *self, LogSource *source)
{
  self->super.source = source;
  source->ack_tracker = (AckTracker *)self;
  self->ack_record_storage.super.tracker = (AckTracker *)self;
  _setup_callbacks(self);
}

AckTracker *
instant_ack_tracker_bookmarkless_new(LogSource *source)
{
  InstantAckTrackerBookmarkless *self = (InstantAckTrackerBookmarkless *)g_new0(InstantAckTrackerBookmarkless, 1);

  _init_instance(self, source);

  return (AckTracker *)self;
}
