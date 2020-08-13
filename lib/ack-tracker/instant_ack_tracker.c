/*
 * Copyright (c) 2020 One Identity
 * Copyright (c) 2020 Laszlo Budai <laszlo.budai@outlook.com>
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
#include "ack_tracker.h"
#include "bookmark.h"
#include "syslog-ng.h"

typedef struct _InstantAckRecord
{
  AckRecord super;
  void *padding;
} InstantAckRecord;

typedef struct _InstantAckTracker
{
  AckTracker super;
  InstantAckRecord *pending_ack_record;
} InstantAckTracker;

static Bookmark *
_request_bookmark(AckTracker *s)
{
  InstantAckTracker *self = (InstantAckTracker *)s;
  if (!self->pending_ack_record)
    self->pending_ack_record = g_new0(InstantAckRecord, 1);

  return &(self->pending_ack_record->super.bookmark);
}

static void
_track_msg(AckTracker *s, LogMessage *msg)
{
  InstantAckTracker *self = (InstantAckTracker *)s;
  log_pipe_ref((LogPipe *)self->super.source);
  self->pending_ack_record->super.bookmark.persist_state = s->source->super.cfg->state;
  self->pending_ack_record->super.tracker = s;
  msg->ack_record = (AckRecord *) self->pending_ack_record;
  self->pending_ack_record = NULL;
}

static void
_save_bookmark(LogMessage *msg)
{
  bookmark_save(&msg->ack_record->bookmark);
}

static void
_ack_record_free(AckRecord *s)
{
  bookmark_destroy(&s->bookmark);

  g_free(s);
}

static void
_manage_msg_ack(AckTracker *s, LogMessage *msg, AckType ack_type)
{
  InstantAckTracker *self = (InstantAckTracker *)s;

  _save_bookmark(msg);
  _ack_record_free(msg->ack_record);
  log_source_flow_control_adjust(self->super.source, 1);

  if (ack_type == AT_SUSPENDED)
    log_source_flow_control_suspend(self->super.source);

  log_msg_unref(msg);
  log_pipe_unref((LogPipe *)self->super.source);
}

static void
_free(AckTracker *s)
{
  InstantAckTracker *self = (InstantAckTracker *)s;
  if (self->pending_ack_record)
    g_free(self->pending_ack_record);

  g_free(self);
}

static void
_setup_callbacks(InstantAckTracker *self)
{
  self->super.request_bookmark = _request_bookmark;
  self->super.track_msg = _track_msg;
  self->super.manage_msg_ack = _manage_msg_ack;
  self->super.free_fn = _free;
}

static void
_init_instance(InstantAckTracker *self, LogSource *source)
{
  self->super.source = source;
  source->ack_tracker = (AckTracker *)self;
  self->pending_ack_record = NULL;
  _setup_callbacks(self);
}

AckTracker *
instant_ack_tracker_new(LogSource *source)
{
  InstantAckTracker *self = (InstantAckTracker *)g_new0(InstantAckTracker, 1);

  _init_instance(self, source);

  return (AckTracker *)self;
}

