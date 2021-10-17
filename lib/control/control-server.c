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
#include "control-commands.h"
#include "messages.h"
#include "str-utils.h"
#include "secret-storage/secret-storage.h"
#include "thread-utils.h"

#include <string.h>
#include <errno.h>

#include <iv.h>
#include <iv_event.h>

typedef struct _ThreadedCommandRunner
{
  ControlConnection *connection;
  GString *command;
  gpointer user_data;

  GThread *thread;
  struct
  {
    GMutex tid_saved_lock;
    GCond tid_saved_cond;
    gboolean tid_saved;
    ThreadId tid;
    GMutex state_lock;
    gboolean cancelled;
    gboolean finished;
  } real_thread;
  ControlConnectionCommand func;
  GString *response;
  struct iv_event response_received;
} ThreadedCommandRunner;

static ThreadedCommandRunner *
_thread_command_runner_new(ControlConnection *cc, GString *cmd, gpointer user_data)
{
  ThreadedCommandRunner *self = g_new0(ThreadedCommandRunner, 1);
  self->connection = cc;
  self->command = g_string_new(cmd->str);
  self->user_data = user_data;
  g_mutex_init(&self->real_thread.tid_saved_lock);
  g_cond_init(&self->real_thread.tid_saved_cond);
  g_mutex_init(&self->real_thread.state_lock);
  self->real_thread.tid_saved = FALSE;
  self->real_thread.tid = 0;

  return self;
}

static void
_thread_command_runner_free(ThreadedCommandRunner *self)
{
  g_mutex_clear(&self->real_thread.tid_saved_lock);
  g_cond_clear(&self->real_thread.tid_saved_cond);
  g_mutex_clear(&self->real_thread.state_lock);
  g_string_free(self->command, TRUE);
  g_free(self);
}

static void
_thread_command_runner_wait_for_tid_saved(ThreadedCommandRunner *self)
{
  g_mutex_lock(&self->real_thread.tid_saved_lock);
  while (self->real_thread.tid_saved == FALSE)
    g_cond_wait(&self->real_thread.tid_saved_cond, &self->real_thread.tid_saved_lock);
  g_mutex_unlock(&self->real_thread.tid_saved_lock);
}

static void
_thread_command_runner_save_tid(ThreadedCommandRunner *self)
{
  g_mutex_lock(&self->real_thread.tid_saved_lock);
  self->real_thread.tid = get_thread_id();
  self->real_thread.tid_saved = TRUE;
  g_cond_broadcast(&self->real_thread.tid_saved_cond);
  g_mutex_unlock(&self->real_thread.tid_saved_lock);
}

static void
_send_response(gpointer user_data)
{
  ThreadedCommandRunner *self = (ThreadedCommandRunner *) user_data;
  g_thread_join(self->thread);
  control_connection_send_reply(self->connection, self->response);
  iv_event_unregister(&self->response_received);
  ControlServer *server = self->connection->server;
  server->worker_threads = g_list_remove(server->worker_threads, self);
  _thread_command_runner_free(self);
}

static void
_thread(gpointer user_data)
{
  ThreadedCommandRunner *self = (ThreadedCommandRunner *)user_data;
  _thread_command_runner_save_tid(self);
  self->response = self->func(self->connection, self->command, self->user_data);
  g_mutex_lock(&self->real_thread.state_lock);
  {
    self->real_thread.finished = TRUE;
    if (!self->real_thread.cancelled)
      iv_event_post(&self->response_received);
  }
  g_mutex_unlock(&self->real_thread.state_lock);
}

static void
_thread_command_runner_sync_run(ThreadedCommandRunner *self, ControlConnectionCommand func)
{
  msg_warning("Cannot start a separated thread - ControlServer is not running",
              evt_tag_str("command", self->command->str));
  control_connection_send_reply(self->connection, func(self->connection, self->command, self->user_data));
  _thread_command_runner_free(self);
}

static void
_thread_command_runner_run(ThreadedCommandRunner *self, ControlConnectionCommand func)
{
  IV_EVENT_INIT(&self->response_received);
  self->response_received.handler = _send_response;
  self->response_received.cookie = self;
  self->func = func;

  if (!main_loop_is_control_server_running(main_loop_get_instance()))
    {
      _thread_command_runner_sync_run(self, func);
      return;
    }

  iv_event_register(&self->response_received);

  self->thread = g_thread_new(self->command->str, (GThreadFunc) _thread, self);
  _thread_command_runner_wait_for_tid_saved(self);
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
  g_assert(self->real_thread.tid_saved == TRUE);
  gboolean has_to_free = FALSE;

  g_mutex_lock(&self->real_thread.state_lock);
  {
    self->real_thread.cancelled = TRUE;
    if (!self->real_thread.finished)
      {
        thread_cancel(self->real_thread.tid);
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
  control_connection_free(cc);
}

void
control_server_init_instance(ControlServer *self, const gchar *path)
{
  self->control_socket_name = g_strdup(path);
  self->worker_threads = NULL;
}

void
control_server_free(ControlServer *self)
{
  // it's not racy as the iv_main() is executed by this thread
  // posted events are not executed as iv_quit() is already called (before control_server_free)
  // but it is possible that some ThreadedCommandRunner::thread are still running
  if (self->worker_threads)
    g_list_free_full(self->worker_threads, _delete_thread_command_runner);

  if (self->free_fn)
    {
      self->free_fn(self);
    }
  g_free(self->control_socket_name);
  g_free(self);
}

void
control_connection_free(ControlConnection *self)
{
  if (self->free_fn)
    {
      self->free_fn(self);
    }
  g_string_free(self->output_buffer, TRUE);
  g_string_free(self->input_buffer, TRUE);
  g_free(self);
}

void
control_connection_send_reply(ControlConnection *self, GString *reply)
{
  g_string_assign(self->output_buffer, reply->str);
  g_string_free(reply, TRUE);

  self->pos = 0;
  self->waiting_for_output = FALSE;

  g_assert(self->output_buffer->len > 0);

  if (self->output_buffer->str[self->output_buffer->len - 1] != '\n')
    {
      g_string_append_c(self->output_buffer, '\n');
    }
  g_string_append(self->output_buffer, ".\n");

  control_connection_update_watches(self);
}

static void
control_connection_io_output(gpointer s)
{
  ControlConnection *self = (ControlConnection *) s;
  gint rc;

  rc = self->write(self, self->output_buffer->str + self->pos, self->output_buffer->len - self->pos);
  if (rc < 0)
    {
      if (errno != EAGAIN)
        {
          msg_error("Error writing control channel",
                    evt_tag_error("error"));
          control_server_connection_closed(self->server, self);
          return;
        }
    }
  else
    {
      self->pos += rc;
    }
  control_connection_update_watches(self);
}

void
control_connection_wait_for_output(ControlConnection *self)
{
  if (self->output_buffer->len == 0)
    self->waiting_for_output = TRUE;
  control_connection_update_watches(self);
}

static void
control_connection_io_input(void *s)
{
  ControlConnection *self = (ControlConnection *) s;
  GString *command = NULL;
  gchar *nl;
  gint rc;
  gint orig_len;
  GList *iter;

  if (self->input_buffer->len > MAX_CONTROL_LINE_LENGTH)
    {
      /* too much data in input, drop the connection */
      msg_error("Too much data in the control socket input buffer");
      control_server_connection_closed(self->server, self);
      return;
    }

  orig_len = self->input_buffer->len;

  /* NOTE: plus one for the terminating NUL */
  g_string_set_size(self->input_buffer, self->input_buffer->len + 128 + 1);
  rc = self->read(self, self->input_buffer->str + orig_len, 128);
  if (rc < 0)
    {
      if (errno != EAGAIN)
        {
          msg_error("Error reading command on control channel, closing control channel",
                    evt_tag_error("error"));
          goto destroy_connection;
        }
      /* EAGAIN, should try again when data comes */
      control_connection_update_watches(self);
      return;
    }
  else if (rc == 0)
    {
      msg_debug("EOF on control channel, closing connection");
      goto destroy_connection;
    }
  else
    {
      self->input_buffer->len = orig_len + rc;
      self->input_buffer->str[self->input_buffer->len] = 0;
    }

  /* here we have finished reading the input, check if there's a newline somewhere */
  nl = strchr(self->input_buffer->str, '\n');
  if (nl)
    {
      command = g_string_sized_new(128);
      /* command doesn't contain NL */
      g_string_assign_len(command, self->input_buffer->str, nl - self->input_buffer->str);
      secret_storage_wipe(self->input_buffer->str, nl - self->input_buffer->str);
      /* strip NL */
      /*g_string_erase(self->input_buffer, 0, command->len + 1);*/
      g_string_truncate(self->input_buffer, 0);
    }
  else
    {
      /* no EOL in the input buffer, wait for more data */
      control_connection_update_watches(self);
      return;
    }

  iter = g_list_find_custom(get_control_command_list(), command->str,
                            (GCompareFunc)control_command_start_with_command);
  if (iter == NULL)
    {
      msg_error("Unknown command read on control channel, closing control channel",
                evt_tag_str("command", command->str));
      g_string_free(command, TRUE);
      goto destroy_connection;
    }
  ControlCommand *cmd_desc = (ControlCommand *) iter->data;

  cmd_desc->func(self, command, cmd_desc->user_data);
  control_connection_wait_for_output(self);

  secret_storage_wipe(command->str, command->len);
  g_string_free(command, TRUE);
  return;
destroy_connection:
  control_server_connection_closed(self->server, self);
}

void
control_connection_init_instance(ControlConnection *self, ControlServer *server)
{
  self->server = server;
  self->output_buffer = g_string_sized_new(256);
  self->input_buffer = g_string_sized_new(128);
  self->handle_input = control_connection_io_input;
  self->handle_output = control_connection_io_output;
  return;
}


void
control_connection_start_watches(ControlConnection *self)
{
  if (self->events.start_watches)
    self->events.start_watches(self);
}

void
control_connection_update_watches(ControlConnection *self)
{
  if (self->events.update_watches)
    self->events.update_watches(self);
}

void
control_connection_stop_watches(ControlConnection *self)
{
  if (self->events.stop_watches)
    self->events.stop_watches(self);
}
