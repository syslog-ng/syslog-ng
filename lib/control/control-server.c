/*
 * Copyright (c) 2002-2014 Balabit
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

#include "control-server.h"
#include "control-connection.h"
#include "control-command-thread.h"
#include "messages.h"


void
_cancel_worker(gpointer data, gpointer user_data)
{
  ControlCommandThread *thread = (ControlCommandThread *) data;

  msg_warning("Requesting the cancellation of control command thread",
              evt_tag_str("control_command", control_command_thread_get_command(thread)));
  control_command_thread_cancel(thread);

  /* NOTE: threads would call control_server_worker_finished() when done (at
   * least if the main loop is still executing once to process our
   * thread_finished event).
   *
   * If finished() is called, our ref stored on the worker_threads list is
   * dropped at that point.
   *
   * If finished() is not called (e.g.  the workers are stuck and have not
   * exited until our mainloop exit strategy is executed), their reference
   * remains lingering on the worker_threads list.
   *
   * We try to take care of such lingering refs in control_server_stop(),
   * which is executed already after iv_main() returns.
   */
}

void
control_server_cancel_workers(ControlServer *self)
{
  if (self->worker_threads)
    {
      msg_debug("Cancelling control server worker threads");
      g_list_foreach(self->worker_threads, _cancel_worker, NULL);
      msg_debug("Control server worker threads have been cancelled");
    }
}

void
control_server_worker_started(ControlServer *self, ControlCommandThread *worker)
{
  control_command_thread_ref(worker);
  self->worker_threads = g_list_append(self->worker_threads, worker);
}

void
control_server_worker_finished(ControlServer *self, ControlCommandThread *worker)
{
  self->worker_threads = g_list_remove(self->worker_threads, worker);
  control_command_thread_unref(worker);
}

void
control_server_connection_closed(ControlServer *self, ControlConnection *cc)
{
  control_connection_stop_watches(cc);
  control_connection_unref(cc);
}

gboolean
control_server_start_method(ControlServer *self)
{
  /* NOTE: this is a placeholder and is empty for now.  Derived
   * ControlServer implementations must call this at startup */
  return TRUE;
}

void
_unref_worker(gpointer data)
{
  ControlCommandThread *thread = (ControlCommandThread *) data;

  msg_warning("Control command thread has not exited by the time we need to exit, forcing",
              evt_tag_str("control_command", control_command_thread_get_command(thread)));
  control_command_thread_unref(thread);
}

/* NOTE: this is called once the mainloop has exited, thus we would not get
 * our worker_thread_finished() callbacks anymore.  If our worker_threads is
 * not empty at this point, then they have not yet responded to our cancel
 * signal earlier and they won't as no further main loop iterations will be
 * performed.
 *
 * At this point we can unref the elements of the worker list, the thread
 * itself will keep a reference until it is done executing, but we won't get
 * notifications anymore.  If the thread is still running at the point
 * _exit() is called, we would leak some ControlCommandThread instances.
 */
void
control_server_stop_method(ControlServer *self)
{
  if (self->worker_threads)
    {
      g_list_free_full(self->worker_threads, _unref_worker);
      self->worker_threads = NULL;
    }
}

void
control_server_free_method(ControlServer *self)
{
}

void
control_server_init_instance(ControlServer *self)
{
  self->worker_threads = NULL;
  self->start = control_server_start_method;
  self->stop = control_server_stop_method;
  self->free_fn = control_server_free_method;
}

void
control_server_free(ControlServer *self)
{
  g_assert(self->worker_threads == NULL);
  if (self->free_fn)
    {
      self->free_fn(self);
    }
  g_free(self);
}
