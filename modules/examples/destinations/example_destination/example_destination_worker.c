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
#include "thread-utils.h"

#include <stdio.h>

static LogThreadedResult
_dw_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  ExampleDestinationWorker *self = (ExampleDestinationWorker *)s;

  GString *string_to_write = g_string_new("");
  g_string_printf(string_to_write, "thread_id=%lu message=%s\n",
                  self->thread_id, log_msg_get_value(msg, LM_V_MESSAGE, NULL));

  size_t retval = fwrite(string_to_write->str, 1, string_to_write->len, self->file);
  if (retval != string_to_write->len)
    {
      msg_error("Error while reading file");
      return LTR_NOT_CONNECTED;
    }

  if (fflush(self->file) != 0)
    {
      msg_error("Error while flushing file");
      return LTR_NOT_CONNECTED;
    }

  g_string_free(string_to_write, TRUE);

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
  ExampleDestinationWorker *self = (ExampleDestinationWorker *)s;
  ExampleDestinationDriver *owner = (ExampleDestinationDriver *) s->owner;

  self->file = fopen(owner->filename->str, "a");
  if (!self->file)
    {
      msg_error("Could not open file", evt_tag_error("error"));
      return FALSE;
    }

  return TRUE;
}

static void
_disconnect(LogThreadedDestWorker *s)
{
  ExampleDestinationWorker *self = (ExampleDestinationWorker *)s;

  fclose(self->file);
}

static gboolean
_thread_init(LogThreadedDestWorker *s)
{
  ExampleDestinationWorker *self = (ExampleDestinationWorker *)s;

  /*
    You can create thread specific resources here. In this example, we
    store the thread id.
  */
  self->thread_id = get_thread_id();

  return log_threaded_dest_worker_init_method(s);
}

static void
_thread_deinit(LogThreadedDestWorker *s)
{
  /*
    If you created resources during _thread_init,
    you need to free them here
  */

  log_threaded_dest_worker_deinit_method(s);
}

static void
_dw_free(LogThreadedDestWorker *s)
{
  /*
    If you created resources during new,
    you need to free them here.
  */

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
