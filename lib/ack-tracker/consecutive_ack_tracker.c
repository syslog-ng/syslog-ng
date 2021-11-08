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

#include "consecutive_ack_tracker.h"
#include "consecutive_ack_record_container.h"
#include "bookmark.h"
#include "syslog-ng.h"

typedef struct _ConsecutiveAckTracker
{
  AckTracker super;
  ConsecutiveAckRecord *pending_ack_record;
  ConsecutiveAckRecordContainer *ack_records;
  GMutex mutex;
  AckTrackerOnAllAcked on_all_acked;
  gboolean bookmark_saving_disabled;
} ConsecutiveAckTracker;

void
consecutive_ack_tracker_lock(AckTracker *s)
{
  ConsecutiveAckTracker *self = (ConsecutiveAckTracker *)s;
  g_mutex_lock(&self->mutex);
}

void
consecutive_ack_tracker_unlock(AckTracker *s)
{
  ConsecutiveAckTracker *self = (ConsecutiveAckTracker *)s;
  g_mutex_unlock(&self->mutex);
}

void
consecutive_ack_tracker_set_on_all_acked(AckTracker *s, AckTrackerOnAllAckedFunc func, gpointer user_data,
                                         GDestroyNotify user_data_free_fn)
{
  ConsecutiveAckTracker *self = (ConsecutiveAckTracker *)s;
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
consecutive_ack_tracker_on_all_acked_call(AckTracker *s)
{
  ConsecutiveAckTracker *self = (ConsecutiveAckTracker *)s;
  AckTrackerOnAllAcked *handler = &self->on_all_acked;
  if (handler->func)
    {
      handler->func(handler->user_data);
    }
}

static void
_ack_records_track_msg(ConsecutiveAckTracker *self, LogMessage *msg)
{
  consecutive_ack_record_container_store_pending(self->ack_records);

  msg->ack_record = (AckRecord *) self->pending_ack_record;
  self->pending_ack_record = NULL;
}

static void
consecutive_ack_tracker_track_msg(AckTracker *s, LogMessage *msg)
{
  ConsecutiveAckTracker *self = (ConsecutiveAckTracker *)s;
  LogSource *source = self->super.source;

  g_assert(self->pending_ack_record != NULL);

  log_pipe_ref((LogPipe *)source);

  consecutive_ack_tracker_lock(s);
  {
    _ack_records_track_msg(self, msg);
  }
  consecutive_ack_tracker_unlock(s);
}

static gboolean
_is_bookmark_saving_enabled(ConsecutiveAckTracker *self)
{
  return self->bookmark_saving_disabled == FALSE;
}

static void
_ack_record_save_bookmark(ConsecutiveAckRecord *ack_record)
{
  Bookmark *bookmark = &(ack_record->super.bookmark);

  bookmark_save(bookmark);
}

static guint32
_ack_records_untrack_msg(ConsecutiveAckTracker *self, LogMessage *msg, AckType ack_type)
{
  guint32 ack_range_length = consecutive_ack_record_container_get_continual_range_length(self->ack_records);
  if (ack_range_length > 0)
    {
      if (ack_type != AT_ABORTED && _is_bookmark_saving_enabled(self))
        {
          _ack_record_save_bookmark(consecutive_ack_record_container_at(self->ack_records, ack_range_length - 1));
        }
      consecutive_ack_record_container_drop(self->ack_records, ack_range_length);
    }

  return ack_range_length;
}

static void
consecutive_ack_tracker_manage_msg_ack(AckTracker *s, LogMessage *msg, AckType ack_type)
{
  ConsecutiveAckTracker *self = (ConsecutiveAckTracker *)s;
  ConsecutiveAckRecord *ack_rec = (ConsecutiveAckRecord *)msg->ack_record;
  ack_rec->acked = TRUE;

  if (ack_type == AT_SUSPENDED)
    log_source_flow_control_suspend(self->super.source);

  consecutive_ack_tracker_lock(s);
  {
    guint32 ack_range_length = _ack_records_untrack_msg(self, msg, ack_type);
    if (ack_range_length > 0)
      {
        if (ack_type == AT_SUSPENDED)
          log_source_flow_control_adjust_when_suspended(self->super.source, ack_range_length);
        else
          log_source_flow_control_adjust(self->super.source, ack_range_length);

        if (consecutive_ack_tracker_is_empty(s))
          consecutive_ack_tracker_on_all_acked_call(s);
      }
  }
  consecutive_ack_tracker_unlock(s);

  log_msg_unref(msg);
  log_pipe_unref((LogPipe *)self->super.source);
}

gboolean
consecutive_ack_tracker_is_empty(AckTracker *s)
{
  ConsecutiveAckTracker *self = (ConsecutiveAckTracker *)s;

  return consecutive_ack_record_container_is_empty(self->ack_records);
}

static Bookmark *
consecutive_ack_tracker_request_bookmark(AckTracker *s)
{
  ConsecutiveAckTracker *self = (ConsecutiveAckTracker *)s;

  if (!self->pending_ack_record)
    {
      consecutive_ack_tracker_lock(s);
      self->pending_ack_record = consecutive_ack_record_container_request_pending(self->ack_records);
      consecutive_ack_tracker_unlock(s);
    }

  if (self->pending_ack_record)
    {
      self->pending_ack_record->super.bookmark.persist_state = s->source->super.cfg->state;

      self->pending_ack_record->super.tracker = (AckTracker *)self;

      return &(self->pending_ack_record->super.bookmark);
    }

  return NULL;
}

static void
consecutive_ack_tracker_free(AckTracker *s)
{
  ConsecutiveAckTracker *self = (ConsecutiveAckTracker *)s;
  AckTrackerOnAllAcked *handler = &self->on_all_acked;

  if (handler->user_data_free_fn && handler->user_data)
    {
      handler->user_data_free_fn(handler->user_data);
    }

  g_mutex_clear(&self->mutex);

  consecutive_ack_record_container_free(self->ack_records);

  g_free(self);
}

static void
consecutive_ack_tracker_disable_bookmark_saving(AckTracker *s)
{
  ConsecutiveAckTracker *self = (ConsecutiveAckTracker *)s;

  consecutive_ack_tracker_lock(s);
  {
    self->bookmark_saving_disabled = TRUE;
  }
  consecutive_ack_tracker_unlock(s);
}

static void
_setup_callbacks(ConsecutiveAckTracker *self)
{
  self->super.request_bookmark = consecutive_ack_tracker_request_bookmark;
  self->super.track_msg = consecutive_ack_tracker_track_msg;
  self->super.manage_msg_ack = consecutive_ack_tracker_manage_msg_ack;
  self->super.disable_bookmark_saving = consecutive_ack_tracker_disable_bookmark_saving;
  self->super.free_fn = consecutive_ack_tracker_free;
}

static void
consecutive_ack_tracker_init_instance(ConsecutiveAckTracker *self, LogSource *source,
                                      ConsecutiveAckRecordContainer *ack_records)
{
  self->super.source = source;
  source->ack_tracker = (AckTracker *)self;
  self->ack_records = ack_records;
  g_mutex_init(&self->mutex);
  _setup_callbacks(self);
}

static ConsecutiveAckRecordContainer *
_create_ack_record_container(LogSource *source)
{
  if (log_source_is_dynamic_window_enabled(source))
    return consecutive_ack_record_container_dynamic_new();

  return consecutive_ack_record_container_static_new(log_source_get_init_window_size(source));
}

AckTracker *
consecutive_ack_tracker_new(LogSource *source)
{
  ConsecutiveAckTracker *self = g_new0(ConsecutiveAckTracker, 1);
  ConsecutiveAckRecordContainer *ack_records = _create_ack_record_container(source);

  consecutive_ack_tracker_init_instance(self, source, ack_records);

  return (AckTracker *)self;
}
