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
#include "mainloop.h"
#include "messages.h"

typedef struct _ThreadedCommandRunner
{
  ControlConnection *connection;
  GString *command;
  gpointer user_data;

  GThread *thread;
  struct
  {
    GMutex state_lock;
    gboolean cancelled;
    gboolean finished;
  } real_thread;
  ControlConnectionCommand func;
  struct iv_event thread_finished;
} ThreadedCommandRunner;

static ThreadedCommandRunner *
_thread_command_runner_new(ControlConnection *cc, GString *cmd, gpointer user_data)
{
  ThreadedCommandRunner *self = g_new0(ThreadedCommandRunner, 1);
  self->connection = control_connection_ref(cc);
  self->command = g_string_new(cmd->str);
  self->user_data = user_data;

  g_mutex_init(&self->real_thread.state_lock);

  return self;
}

static void
_thread_command_runner_free(ThreadedCommandRunner *self)
{
  g_mutex_clear(&self->real_thread.state_lock);
  g_string_free(self->command, TRUE);
  control_connection_unref(self->connection);
  g_free(self);
}

static void
_on_thread_finished(gpointer user_data)
{
  ThreadedCommandRunner *self = (ThreadedCommandRunner *) user_data;
  g_thread_join(self->thread);
  iv_event_unregister(&self->thread_finished);
  ControlServer *server = self->connection->server;
  server->worker_threads = g_list_remove(server->worker_threads, self);
  _thread_command_runner_free(self);
}

static void
_thread(gpointer user_data)
{
  ThreadedCommandRunner *self = (ThreadedCommandRunner *)user_data;
  GString *response = self->func(self->connection, self->command, self->user_data);
  if (response)
    control_connection_send_reply(self->connection, response);
  g_mutex_lock(&self->real_thread.state_lock);
  {
    self->real_thread.finished = TRUE;
    if (!self->real_thread.cancelled)
      iv_event_post(&self->thread_finished);
  }
  g_mutex_unlock(&self->real_thread.state_lock);
}

static void
_thread_command_runner_sync_run(ThreadedCommandRunner *self, ControlConnectionCommand func)
{
  msg_warning("Cannot start a separated thread - ControlServer is not running",
              evt_tag_str("command", self->command->str));

  GString *response = func(self->connection, self->command, self->user_data);
  if (response)
    control_connection_send_reply(self->connection, response);

  _thread_command_runner_free(self);
}

static void
_thread_command_runner_run(ThreadedCommandRunner *self, ControlConnectionCommand func)
{
  IV_EVENT_INIT(&self->thread_finished);
  self->thread_finished.handler = _on_thread_finished;
  self->thread_finished.cookie = self;
  self->func = func;

  if (!main_loop_is_control_server_running(main_loop_get_instance()))
    {
      _thread_command_runner_sync_run(self, func);
      return;
    }

  iv_event_register(&self->thread_finished);

  self->thread = g_thread_new(self->command->str, (GThreadFunc) _thread, self);
  ControlServer *server = self->connection->server;
  server->worker_threads = g_list_append(server->worker_threads, self);
}

void
control_connection_start_as_thread(ControlConnection *self, ControlConnectionCommand cmd_cb,
                                   GString *command, gpointer user_data)
{
  ThreadedCommandRunner *runner = _thread_command_runner_new(self, command, user_data);
  _thread_command_runner_run(runner, cmd_cb);
}

static void
_delete_thread_command_runner(gpointer data)
{
  ThreadedCommandRunner *self = (ThreadedCommandRunner *) data;
  gboolean has_to_free = FALSE;

  g_mutex_lock(&self->real_thread.state_lock);
  {
    self->real_thread.cancelled = TRUE;
    if (!self->real_thread.finished)
      {
        has_to_free = TRUE;
      }
  }
  g_mutex_unlock(&self->real_thread.state_lock);

  if (has_to_free)
    {
      g_thread_join(self->thread);
      _thread_command_runner_free(self);
    }
}

void
control_server_cancel_workers(ControlServer *self)
{
  if (self->worker_threads)
    {
      self->cancelled = TRUE; // racy, but it's okay
      msg_warning("Cancelling control server worker threads");
      g_list_free_full(self->worker_threads, _delete_thread_command_runner);
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
  // but it is possible that some ThreadedCommandRunner::thread are still running
  if (self->worker_threads)
    {
      g_list_free_full(self->worker_threads, _delete_thread_command_runner);
      self->worker_threads = NULL;
    }
}


void
control_server_init_instance(ControlServer *self)
{
  self->worker_threads = NULL;
  self->cancelled = FALSE;
  self->start = control_server_start_method;
  self->stop = control_server_stop_method;
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
