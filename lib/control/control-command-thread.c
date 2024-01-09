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
#include "secret-storage/secret-storage.h"
#include "scratch-buffers.h"
#include <iv_event.h>

struct _ControlCommandThread
{
  GAtomicCounter ref_cnt;
  ControlConnection *connection;
  GString *command;
  gpointer user_data;

  GThread *thread;
  GMutex state_lock;
  gboolean cancelled;
  gboolean finished;
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

  /* drop ref belonging to the thread_finished event */
  control_command_thread_unref(self);
}

static void
_thread(gpointer user_data)
{
  ControlCommandThread *self = (ControlCommandThread *) user_data;

  iv_init();
  scratch_buffers_allocator_init();

  msg_debug("Control command thread has started",
            evt_tag_str("control_command", self->command->str));
  self->func(self->connection, self->command, self->user_data, &self->cancelled);
  g_mutex_lock(&self->state_lock);
  {
    self->finished = TRUE;
    if (!self->cancelled)
      iv_event_post(&self->thread_finished);
  }
  g_mutex_unlock(&self->state_lock);

  /* drop ref belonging to the thread function */
  msg_debug("Control command thread is exiting now",
            evt_tag_str("control_command", self->command->str));

  scratch_buffers_explicit_gc();
  scratch_buffers_allocator_deinit();
  control_command_thread_unref(self);
  iv_deinit();
}

const gchar *
control_command_thread_get_command(ControlCommandThread *self)
{
  return self->command->str;
}

void
control_command_thread_run(ControlCommandThread *self)
{
  self->thread_finished.cookie = control_command_thread_ref(self);
  iv_event_register(&self->thread_finished);

  control_server_worker_started(self->connection->server, self);
  self->thread = g_thread_new(self->command->str, (GThreadFunc) _thread, control_command_thread_ref(self));
}

void
control_command_thread_cancel(ControlCommandThread *self)
{
  gboolean has_to_join = FALSE;

  g_mutex_lock(&self->state_lock);
  {
    self->cancelled = TRUE;
    if (!self->finished)
      {
        has_to_join = TRUE;
      }
  }
  g_mutex_unlock(&self->state_lock);

  if (has_to_join)
    g_thread_join(self->thread);
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

  g_mutex_init(&self->state_lock);

  IV_EVENT_INIT(&self->thread_finished);
  self->thread_finished.handler = _on_thread_finished;

  return self;
}

static void
_control_command_thread_free(ControlCommandThread *self)
{
  g_mutex_clear(&self->state_lock);
  secret_storage_wipe(self->command->str, self->command->len);
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
