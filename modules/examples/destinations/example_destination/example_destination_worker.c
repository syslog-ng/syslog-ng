/*
 * Copyright (c) 2020 Balabit
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "example_destination_worker.h"
#include "example_destination.h"

static LogThreadedResult
_dw_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  ExampleDestinationDriver *owner = (ExampleDestinationDriver *) s->owner;
  fprintf(stderr, "Message sent using file name: %s\n", owner->filename);

  return LTR_SUCCESS;
  /*
   * LTR_DROP,
   * LTR_ERROR,
   * LTR_SUCCESS,
   * LTR_QUEUED,
   * LTR_NOT_CONNECTED,
   * LTR_RETRY,
  */
}

static gboolean
_connect(LogThreadedDestWorker *s)
{
  msg_debug("Connection succeeded",
            evt_tag_str("driver", s->owner->super.super.id), NULL);

  return TRUE;
}

static void
_disconnect(LogThreadedDestWorker *s)
{
  msg_debug("Connection closed",
            evt_tag_str("driver", s->owner->super.super.id), NULL);
}

static gboolean
_thread_init(LogThreadedDestWorker *s)
{
  msg_debug("Worker thread started",
            evt_tag_str("driver", s->owner->super.super.id),
            NULL);

  return log_threaded_dest_worker_init_method(s);
}

static void
_thread_deinit(LogThreadedDestWorker *s)
{
  msg_debug("Worker thread stopped",
            evt_tag_str("driver", s->owner->super.super.id),
            NULL);

  log_threaded_dest_worker_deinit_method(s);
}

static void
_dw_free(LogThreadedDestWorker *s)
{
  msg_debug("Worker free method called",
            evt_tag_str("driver", s->owner->super.super.id),
            NULL);

  log_threaded_dest_worker_free_method(s);
}

LogThreadedDestWorker *
example_destination_dw_new(LogThreadedDestDriver *o, gint worker_index)
{
  ExampleDestinationWorker *self = g_new0(ExampleDestinationWorker, 1);

  log_threaded_dest_worker_init_instance(&self->super, o, worker_index);
  self->super.thread_init = _thread_init;
  self->super.thread_deinit = _thread_deinit;
  self->super.insert = _dw_insert;
  self->super.free_fn = _dw_free;
  self->super.connect = _connect;
  self->super.disconnect = _disconnect;

  return &self->super;
}
