/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Viktor Tusa <tusa@balabit.hu>
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

#include "stomp.h"
#include "host-resolve.h"
#include "str-utils.h"
#include "messages.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>


#define STOMP_PARSE_HEADER 1
#define STOMP_PARSE_DATA 2
#define STOMP_PARSE_ERROR 0

void
stomp_frame_init(stomp_frame *frame, const char *command, int command_len)
{
  frame->command = g_strndup(command, command_len);
  frame->headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  frame->body_length = -1;
  frame->body = NULL;
};

void
stomp_frame_add_header(stomp_frame *frame, const char *name, const char *value)
{
  msg_debug("Adding header",
            evt_tag_str("name",name),
            evt_tag_str("value",value));

  g_hash_table_insert(frame->headers, g_strdup(name), g_strdup(value));
};

void
stomp_frame_add_header_len(stomp_frame *frame, const char *name, int name_len, const char *value, int value_len)
{
  char *name_slice = g_strndup(name, name_len);
  char *value_slice = g_strndup(value, value_len);
  msg_debug("Adding header",
            evt_tag_str("name",name_slice),
            evt_tag_str("value",value_slice));

  g_hash_table_insert(frame->headers, name_slice, value_slice);
};

void
stomp_frame_set_body(stomp_frame *frame, const char *body, int body_len)
{
  frame->body = g_strndup(body, body_len);
  frame->body_length = body_len;
};

int
stomp_frame_deinit(stomp_frame *frame)
{
  g_hash_table_destroy(frame->headers);
  g_free(frame->command);
  g_free(frame->body);

  return TRUE;
}

static void
_stomp_connection_free(stomp_connection *conn)
{
  g_sockaddr_unref(conn->remote_sa);
  g_free(conn);
}

int
stomp_connect(stomp_connection **connection_ref, char *hostname, int port)
{
  stomp_connection *conn;

  conn = g_new0(stomp_connection, 1);

  conn->socket = socket(AF_INET, SOCK_STREAM, 0);
  if (conn->socket == -1)
    {
      msg_error("Failed to create socket!");
      return FALSE;
    }

  if (!resolve_hostname_to_sockaddr(&conn->remote_sa, AF_INET, hostname))
    {
      msg_error("Failed to resolve hostname in stomp driver",
                evt_tag_str("hostname", hostname));

      return FALSE;
    }

  g_sockaddr_set_port(conn->remote_sa, port);
  if (!g_connect(conn->socket, conn->remote_sa))
    {
      msg_error("Stomp connection failed",
                evt_tag_str("host", hostname));
      _stomp_connection_free(conn);
      return FALSE;
    }

  (*connection_ref) = conn;

  return TRUE;
};

int
stomp_disconnect(stomp_connection **connection_ref)
{
  stomp_connection *conn = *connection_ref;

  if (!conn)
    return TRUE;

  shutdown(conn->socket, SHUT_RDWR);
  close(conn->socket);
  _stomp_connection_free(conn);
  *connection_ref = NULL;
  return TRUE;
};

static void
write_header_into_gstring(gpointer key, gpointer value, gpointer userdata)
{
  GString *str = (GString *) userdata;

  if (key == NULL || value == NULL)
    return;

  g_string_append(str, key);
  g_string_append_c(str, ':');
  g_string_append(str, value);
  g_string_append_c(str, '\n');
}

static int
write_gstring_to_socket(int fd, GString *data)
{
  int res = 0;
  int remaining = data->len;

  while ((remaining > 0) && (res >= 0))
    {
      res = write(fd, data->str + (data->len - remaining), remaining);
      if (res > 0)
        remaining = remaining - res;
    }

  if (res < 0)
    {
      msg_error("Error happened during write",
                evt_tag_error("errno"));
      return FALSE;
    }

  return TRUE;
}

static int
stomp_read_data(stomp_connection *connection, GString *buffer)
{
  char tmp_buf[4096];
  int res;

  res = read(connection->socket, tmp_buf, sizeof(tmp_buf));
  if (res < 0)
    return FALSE;

  g_string_assign_len(buffer, tmp_buf, res);
  while (res == sizeof(tmp_buf))
    {
      res = read(connection->socket, tmp_buf, sizeof(tmp_buf));
      g_string_append_len(buffer, tmp_buf, res);
    }
  return TRUE;
}

static int
stomp_parse_command(char *buffer, int buflen, stomp_frame *frame, char **out_pos)
{
  char *pos;

  pos = g_strstr_len(buffer, buflen, "\n");
  if (pos == NULL)
    return STOMP_PARSE_ERROR;

  stomp_frame_init(frame, buffer, pos - buffer);
  *out_pos = pos + 1;

  return STOMP_PARSE_HEADER;
}

static int
stomp_parse_header(char *buffer, int buflen, stomp_frame *frame, char **out_pos)
{
  char *pos, *colon;

  if (buflen <= 1)
    {
      *out_pos = buffer;
      return STOMP_PARSE_DATA;
    }

  pos = g_strstr_len(buffer, buflen, "\n");
  if (!pos)
    return STOMP_PARSE_ERROR;

  if (pos == buffer)
    {
      *out_pos = pos + 1;
      return STOMP_PARSE_DATA;
    }

  colon = g_strstr_len(buffer, pos - buffer, ":");
  if (!colon)
    return STOMP_PARSE_ERROR;

  stomp_frame_add_header_len(frame, buffer, colon - buffer,
                             colon + 1, pos - colon - 1);
  *out_pos = pos + 1;

  return STOMP_PARSE_HEADER;
};

int
stomp_parse_frame(GString *data, stomp_frame *frame)
{
  char *pos;
  int res;

  res = stomp_parse_command(data->str, data->len, frame, &pos);
  if (!res)
    return FALSE;

  res = stomp_parse_header(pos, data->str + data->len - pos, frame, &pos);
  while (res == STOMP_PARSE_HEADER)
    {
      res = stomp_parse_header(pos, data->str + data->len - pos, frame, &pos);
    }

  if (res == STOMP_PARSE_ERROR)
    return FALSE;

  frame->body = g_strndup(pos, data->len - (pos - data->str));
  return TRUE;
}

int
stomp_receive_frame(stomp_connection *connection, stomp_frame *frame)
{
  GString *data = g_string_sized_new(4096);

  if (!stomp_read_data(connection, data))
    {
      g_string_free(data, TRUE);
      return FALSE;
    }

  int res = stomp_parse_frame(data, frame);

  if (res)
    msg_debug("Frame received", evt_tag_str("command", frame->command));

  g_string_free(data, TRUE);
  return res;
}

static int
stomp_check_for_frame(stomp_connection *connection)
{
  struct pollfd pfd;

  pfd.fd = connection->socket;
  pfd.events = POLLIN | POLLPRI;

  poll(&pfd, 1, 0);
  if (pfd.revents & ( POLLIN | POLLPRI))
    {
      stomp_frame frame;

      if (!stomp_receive_frame(connection, &frame))
        return FALSE;
      if (!strcmp(frame.command, "ERROR"))
        {
          msg_error("ERROR frame received from stomp_server");
          stomp_frame_deinit(&frame);
          return FALSE;
        }

      /* According to stomp protocol, here only ERROR or RECEIPT
         frames can come, so we missed a RECEIPT frame here, our
         bad. */
      stomp_frame_deinit(&frame);
      return TRUE;
    }

  return TRUE;
}

GString *
create_gstring_from_frame(stomp_frame *frame)
{
  GString *data = g_string_new("");

  g_string_append(data, frame->command);
  g_string_append_c(data, '\n');
  g_hash_table_foreach(frame->headers, write_header_into_gstring, data);
  g_string_append_c(data, '\n');
  if (frame->body)
    g_string_append_len(data, frame->body, frame->body_length);
  g_string_append_c(data, 0);
  return data;
}

int
stomp_write(stomp_connection *connection, stomp_frame *frame)
{
  GString *data;

  if (!stomp_check_for_frame(connection))
    return FALSE;

  data = create_gstring_from_frame(frame);
  if (!write_gstring_to_socket(connection->socket, data))
    {
      msg_error("Write error, partial write");
      stomp_frame_deinit(frame);
      g_string_free(data, TRUE);
      return FALSE;
    }

  g_string_free(data, TRUE);
  stomp_frame_deinit(frame);
  return TRUE;
}
