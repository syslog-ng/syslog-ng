/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#include "control.h"
#include "gsocket.h"
#include "messages.h"
#include "stats.h"
#include "misc.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <iv.h>

#define MAX_CONTROL_LINE_LENGTH 4096

static gint control_socket;
static struct iv_fd control_listen;

typedef struct _ControlConnection
{
  struct iv_fd control_io;
  GString *input_buffer;
  GString *output_buffer;
  gsize pos;
} ControlConnection;

static void control_connection_update_watches(ControlConnection *self);
static void control_connection_stop_watches(ControlConnection *self);
void control_connection_free(ControlConnection *self);

static void
control_connection_io_output(gpointer s)
{
  ControlConnection *self = (ControlConnection *) s;
  gint rc;
  
  rc = write(self->control_io.fd, self->output_buffer->str + self->pos, self->output_buffer->len - self->pos);
  if (rc < 0)
    {
      if (errno != EAGAIN)
        {
          msg_error("Error writing control channel",
                    evt_tag_errno("error", errno),
                    NULL);
          control_connection_stop_watches(self);
          control_connection_free(self);
          return;
        }
    }
  else
    {
      self->pos += rc;
    }
  control_connection_update_watches(self);
}

static void
control_connection_send_reply(ControlConnection *self, gchar *reply, gboolean free_reply)
{
  g_string_assign(self->output_buffer, reply);
  if (free_reply)
    g_free(reply);

  self->pos = 0;

  if (self->output_buffer->str[self->output_buffer->len - 1] != '\n')
    g_string_append_c(self->output_buffer, '\n');
  g_string_append(self->output_buffer, ".\n");

  control_connection_update_watches(self);
}

static void
control_connection_send_stats(ControlConnection *self, GString *command)
{
  control_connection_send_reply(self, stats_generate_csv(), TRUE);
}

static void
control_connection_message_log(ControlConnection *self, GString *command)
{
  gchar **cmds = g_strsplit(command->str, " ", 3);
  gboolean on;
  int *type = NULL;

  if (!cmds[1])
    {
      control_connection_send_reply(self, "Invalid arguments received, expected at least one argument", FALSE);
      goto exit;
    }

  if (g_str_equal(cmds[1], "DEBUG"))
    type = &debug_flag;
  else if (g_str_equal(cmds[1], "VERBOSE")) 
    type = &verbose_flag;
  else if (g_str_equal(cmds[1], "TRACE")) 
    type = &trace_flag;

  if (type)
    {
      if (cmds[2])
        {
          on = g_str_equal(cmds[2], "ON");
          if (*type != on)
            {
              msg_info("Verbosity changed", evt_tag_str("type", cmds[1]), evt_tag_int("on", on), NULL);
              *type = on;
            }

          control_connection_send_reply(self, "OK", FALSE);
        }
      else
        {
          control_connection_send_reply(self, g_strdup_printf("%s=%d", cmds[1], *type), TRUE);
        }
    }
  else
    control_connection_send_reply(self, "Invalid arguments received", FALSE);

exit:
  g_strfreev(cmds);
}

static struct
{
  const gchar *command;
  const gchar *description;
  void (*func)(ControlConnection *self, GString *command);
} commands[] = 
{
  { "STATS", NULL, control_connection_send_stats },
  { "LOG", NULL, control_connection_message_log },
  { NULL, NULL, NULL },
};

/*
 * NOTE: the channel is not in nonblocking mode, thus the control channel
 * may block syslog-ng completely.
 */
static void
control_connection_io_input(void *s)
{
  ControlConnection *self = (ControlConnection *) s;
  GString *command = NULL;
  gchar *nl;
  gint rc;
  gint cmd;
  gint orig_len;
  
  if (self->input_buffer->len > MAX_CONTROL_LINE_LENGTH)
    {
      /* too much data in input, drop the connection */
      msg_error("Too much data in the control socket input buffer",
                NULL);
      control_connection_stop_watches(self);
      control_connection_free(self);
      return;
    }

  orig_len = self->input_buffer->len;

  /* NOTE: plus one for the terminating NUL */
  g_string_set_size(self->input_buffer, self->input_buffer->len + 128 + 1);
  rc = read(self->control_io.fd, self->input_buffer->str + orig_len, 128);
  if (rc < 0)
    {
      if (errno != EAGAIN)
        {
          msg_error("Error reading command on control channel, closing control channel",
                    evt_tag_errno("error", errno),
                    NULL);
          goto destroy_connection;
        }
      /* EAGAIN, should try again when data comes */
      control_connection_update_watches(self);
      return;
    }
  else if (rc == 0)
    {
      msg_error("EOF on control channel, closing connection",
                NULL);
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
      /* strip NL */
      g_string_erase(self->input_buffer, 0, command->len + 1);
    }
  else
    {
      /* no EOL in the input buffer, wait for more data */
      control_connection_update_watches(self);
      return;
    }

  for (cmd = 0; commands[cmd].func; cmd++)
    {
      if (strncmp(commands[cmd].command, command->str, strlen(commands[cmd].command)) == 0)
        {
          commands[cmd].func(self, command);
          break;
        }
    }
  if (!commands[cmd].func)
    {
      msg_error("Unknown command read on control channel, closing control channel",
                evt_tag_str("command", command->str), NULL);
      goto destroy_connection;
    }
  control_connection_update_watches(self);
  g_string_free(command, TRUE);
  return;
 destroy_connection:
  control_connection_stop_watches(self);
  control_connection_free(self);

}

static void
control_connection_start_watches(ControlConnection *self, gint sock)
{
  IV_FD_INIT(&self->control_io);
  self->control_io.cookie = self;
  self->control_io.fd = sock;
  iv_fd_register(&self->control_io);

  control_connection_update_watches(self);
}

static void
control_connection_stop_watches(ControlConnection *self)
{
  iv_fd_unregister(&self->control_io);
}

static void
control_connection_update_watches(ControlConnection *self)
{
  if (self->output_buffer->len > self->pos)
    {
      iv_fd_set_handler_out(&self->control_io, control_connection_io_output);
      iv_fd_set_handler_in(&self->control_io, NULL);
    }
  else
    {
      iv_fd_set_handler_out(&self->control_io, NULL);
      iv_fd_set_handler_in(&self->control_io, control_connection_io_input);
    }
}

ControlConnection *
control_connection_new(gint sock)
{
  ControlConnection *self = g_new0(ControlConnection, 1);

  self->output_buffer = g_string_sized_new(256);
  self->input_buffer = g_string_sized_new(128);

  control_connection_start_watches(self, sock);
  return self;
}

void
control_connection_free(ControlConnection *self)
{
  close(self->control_io.fd);
  g_string_free(self->output_buffer, TRUE);
  g_string_free(self->input_buffer, TRUE);
  g_free(self);
}


static void
control_socket_accept(gpointer user_data)
{
  gint conn_socket;
  GSockAddr *peer_addr;
  GIOStatus status;
  ControlConnection *conn;
  
  if (control_socket == -1)
    return;
  status = g_accept(control_socket, &conn_socket, &peer_addr);
  if (status != G_IO_STATUS_NORMAL)
    {
      msg_error("Error accepting control socket connection",
                evt_tag_errno("error", errno),
                NULL);
      goto error;
    }
  /* NOTE: the connection will free itself if the peer terminates */
  conn = control_connection_new(conn_socket);
  g_sockaddr_unref(peer_addr);
 error:
  ;
}

void 
control_init(const gchar *control_name)
{
  GSockAddr *saddr;
  
  saddr = g_sockaddr_unix_new(control_name);
  control_socket = socket(PF_UNIX, SOCK_STREAM, 0);
  if (control_socket == -1)
    {
      msg_error("Error opening control socket, external controls will not be available",
               evt_tag_str("socket", control_name),
               NULL);
      return;
    }
  if (g_bind(control_socket, saddr) != G_IO_STATUS_NORMAL)
    {
      msg_error("Error opening control socket, bind() failed",
               evt_tag_str("socket", control_name),
               evt_tag_errno("error", errno),
               NULL);
      goto error;
    }
  if (listen(control_socket, 255) < 0)
    {
      msg_error("Error opening control socket, listen() failed",
               evt_tag_str("socket", control_name),
               evt_tag_errno("error", errno),
               NULL);
      goto error;
    }

  IV_FD_INIT(&control_listen);
  control_listen.fd = control_socket;
  control_listen.handler_in = control_socket_accept;
  iv_fd_register(&control_listen);

  g_sockaddr_unref(saddr);
  return;
 error:
  if (control_socket != -1)
    {
      close(control_socket);
      control_socket = -1;
    }
  g_sockaddr_unref(saddr);
}

void
control_destroy(void)
{
  close(control_socket);
  control_socket = -1;
}
