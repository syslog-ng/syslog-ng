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
control_connection_start_as_thread(ControlConnection *self, ControlCommandFunc cmd_cb,
                                   GString *command, gpointer user_data)
{
  ControlCommandThread *runner = control_command_thread_new(self, command, user_data);
  control_command_thread_run(runner, cmd_cb);
}

void
control_server_cancel_workers(ControlServer *self)
{
  if (self->worker_threads)
    {
      self->cancelled = TRUE; // racy, but it's okay
      msg_warning("Cancelling control server worker threads");
      g_list_free_full(self->worker_threads, _delete_control_command_thread);
      msg_warning("Control server worker threads has been cancelled.");
      self->worker_threads = NULL;
    }
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
control_server_stop_method(ControlServer *self)
{
  // it's not racy as the iv_main() is executed by this thread
  // posted events are not executed as iv_quit() is already called (before control_server_free)
  // but it is possible that some ControlCommandThread::thread are still running
  if (self->worker_threads)
    {
      g_list_free_full(self->worker_threads, _delete_control_command_thread);
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
  self->cancelled = FALSE;
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
