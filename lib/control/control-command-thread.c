/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 2021 Balazs Scheidler <bazsi77@gmail.com>
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
#include "control-command-thread.h"
#include "control-connection.h"
#include "control-server.h"
#include "messages.h"
#include "mainloop.h"
#include <iv_event.h>

struct _ControlCommandThread
{
  GAtomicCounter ref_cnt;
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
  ControlCommandFunc func;
  struct iv_event thread_finished;
};

static void
_on_thread_finished(gpointer user_data)
{
  ControlCommandThread *self = (ControlCommandThread *) user_data;

  g_thread_join(self->thread);
  iv_event_unregister(&self->thread_finished);

  control_server_worker_finished(self->connection->server, self);
  control_command_thread_unref(self);
}

static void
_thread(gpointer user_data)
{
  ControlCommandThread *self = (ControlCommandThread *)user_data;
  self->func(self->connection, self->command, self->user_data);
  g_mutex_lock(&self->real_thread.state_lock);
  {
    self->real_thread.finished = TRUE;
    if (!self->real_thread.cancelled)
      iv_event_post(&self->thread_finished);
  }
  g_mutex_unlock(&self->real_thread.state_lock);
}

static void
control_command_thread_sync_run(ControlCommandThread *self)
{
  msg_warning("Cannot start a separated thread - ControlServer is not running",
              evt_tag_str("command", self->command->str));

  self->func(self->connection, self->command, self->user_data);
  control_command_thread_unref(self);
}

void
control_command_thread_run(ControlCommandThread *self)
{
  if (!main_loop_is_control_server_running(main_loop_get_instance()))
    {
      control_command_thread_sync_run(self);
      return;
    }

  iv_event_register(&self->thread_finished);

  self->thread = g_thread_new(self->command->str, (GThreadFunc) _thread, self);
  ControlServer *server = self->connection->server;
  server->worker_threads = g_list_append(server->worker_threads, self);
}

void
control_command_thread_cancel(ControlCommandThread *self)
{
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
      control_command_thread_unref(self);
    }
}

ControlCommandThread *
control_command_thread_new(ControlConnection *cc, GString *cmd, ControlCommandFunc func, gpointer user_data)
{
  ControlCommandThread *self = g_new0(ControlCommandThread, 1);

  g_atomic_counter_set(&self->ref_cnt, 1);
  self->connection = control_connection_ref(cc);
  self->command = g_string_new(cmd->str);
  self->func = func;
  self->user_data = user_data;

  g_mutex_init(&self->real_thread.state_lock);

  IV_EVENT_INIT(&self->thread_finished);
  self->thread_finished.handler = _on_thread_finished;
  self->thread_finished.cookie = self;

  return self;
}

static void
_control_command_thread_free(ControlCommandThread *self)
{
  g_mutex_clear(&self->real_thread.state_lock);
  g_string_free(self->command, TRUE);
  control_connection_unref(self->connection);
  g_free(self);
}

ControlCommandThread *
control_command_thread_ref(ControlCommandThread *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt) > 0);

  if (self)
    g_atomic_counter_inc(&self->ref_cnt);

  return self;
}

void
control_command_thread_unref(ControlCommandThread *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt));

  if (self && (g_atomic_counter_dec_and_test(&self->ref_cnt)))
    _control_command_thread_free(self);
}
