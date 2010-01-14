/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "control.h"
#include "gsocket.h"
#include "messages.h"
#include "stats.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>

static gint control_socket;

typedef struct _ControlConnectionOutput
{
  GString *buffer;
  gsize pos;
} ControlConnectionOutput;

static gboolean control_channel_input(GIOChannel *channel, GIOCondition cond, gpointer user_data);


static gboolean
control_channel_output(GIOChannel *channel, GIOCondition cond, gpointer user_data)
{
  ControlConnectionOutput *output = (ControlConnectionOutput *) user_data;
  GIOError error;
  gsize bytes_written;
  
  /* NOTE: write is deprecated as it bypasses the output buffer. But using
   * buffered I/O is incompatible with using nonblocking i/o and we don't
   * need buffering anyway
   */
  
  error = g_io_channel_write(channel, output->buffer->str + output->pos, output->buffer->len - output->pos, &bytes_written);
  if (error == G_IO_ERROR_AGAIN)
    return TRUE;
  else if (error != G_IO_ERROR_NONE)
    {
      msg_error("Error writing control channel",
                evt_tag_errno("error", errno),
                NULL);
      return FALSE;
    }
    
  /* normal, some characters were written */
  output->pos += bytes_written;

  if (output->pos == output->buffer->len)
    {
      /**/
      g_string_free(output->buffer, TRUE);
      g_free(output);
      g_io_channel_set_flags(channel, 0, NULL);
      g_io_add_watch(channel, G_IO_IN, control_channel_input, NULL);
      return FALSE;
    }
  return TRUE;
}

static gboolean
control_channel_send_reply(GIOChannel *channel, GString *buffer)
{
  ControlConnectionOutput *output = g_new0(ControlConnectionOutput, 1);
  
  output->buffer = buffer;
  if (output->buffer->str[output->buffer->len - 1] != '\n')
    g_string_append_c(output->buffer, '\n');
  g_string_append(output->buffer, ".\n");
  
  g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL);
  g_io_add_watch(channel, G_IO_OUT, control_channel_output, output);
  return FALSE;
}

static gboolean
control_channel_send_stats(GIOChannel *channel, GString *command)
{
  GString *csv;
  
  csv = stats_generate_csv();
  return control_channel_send_reply(channel, csv);
}

static gboolean
control_channel_message_log(GIOChannel *channel, GString *command)
{
  gchar **cmds = g_strsplit(command->str, " ", 3);
  gboolean on;
  int *type = NULL;

  if (!cmds[1])
    {
      control_channel_send_reply(channel, g_string_new("Invalid arguments received"));
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

          control_channel_send_reply(channel, g_string_new("OK"));
        }
      else
        {
          gchar buff[17];
          snprintf(buff, 16, "%s=%d", cmds[1], *type);
          control_channel_send_reply(channel, g_string_new(buff));
        }
    }
  else
    control_channel_send_reply(channel, g_string_new("Invalid arguments received"));

exit:
  g_strfreev(cmds);
  return TRUE;
}

static struct
{
  const gchar *command;
  const gchar *description;
  gboolean (*func)(GIOChannel *channel, GString *command);
} commands[] = 
{
  { "STATS", NULL, control_channel_send_stats },
  { "LOG", NULL, control_channel_message_log },
  { NULL, NULL, NULL },
};

/*
 * NOTE: the channel is not in nonblocking mode, thus the control channel
 * may block syslog-ng completely.
 */
static gboolean
control_channel_input(GIOChannel *channel, GIOCondition cond, gpointer user_data)
{
  GString *command = g_string_sized_new(32);
  gsize command_len = 0;
  GError *error = NULL;
  gint cmd;
  GIOStatus status;
  
  status = g_io_channel_read_line_string(channel, command, &command_len, &error);
  if (status == G_IO_STATUS_ERROR)
    {
      msg_error("Error reading command on control channel, closing control channel",
                evt_tag_str("error", error->message),
                NULL);
      g_clear_error(&error);
      return FALSE;
    }
  else if (status != G_IO_STATUS_NORMAL)
    {
      /* EAGAIN or EOF */
      msg_verbose("EOF or EAGAIN on control channel, closing control channel", NULL);
      return FALSE;
    }
  /* strip EOL */
  g_string_truncate(command, command_len);

  for (cmd = 0; commands[cmd].func; cmd++)
    {
      if (strncmp(commands[cmd].command, command->str, strlen(commands[cmd].command)) == 0)
        return commands[cmd].func(channel, command);
    }
  msg_error("Unknown command read on control channel, closing control channel",
                evt_tag_str("command", command->str), NULL);
  return FALSE;
}

static gboolean
control_socket_accept(gpointer user_data)
{
  gint conn_socket;
  GSockAddr *peer_addr;
  GIOStatus status;
  GIOChannel *channel;
  
  if (control_socket == -1)
    return FALSE;
  status = g_accept(control_socket, &conn_socket, &peer_addr);
  if (status != G_IO_STATUS_NORMAL)
    {
      msg_error("Error accepting control socket connection",
                evt_tag_errno("error", errno),
                NULL);
      goto error;
    }
  g_sockaddr_unref(peer_addr);
  channel = g_io_channel_unix_new(conn_socket);
  g_io_channel_set_encoding(channel, NULL, NULL);
  g_io_channel_set_close_on_unref(channel, TRUE);
  g_io_add_watch(channel, G_IO_IN, control_channel_input, NULL);
  g_io_channel_unref(channel);
 error:
  return TRUE;
}

void 
control_init(const gchar *control_name, GMainContext *main_context)
{
  GSockAddr *saddr;
  GSource *source;
  
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
  
  source = g_listen_source_new(control_socket);
  g_source_set_callback(source, control_socket_accept, NULL, NULL);
  g_source_attach(source, main_context);
  g_source_unref(source);
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
