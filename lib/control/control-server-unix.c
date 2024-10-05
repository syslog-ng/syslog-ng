/*
 * Copyright (c) 2002-2013 Balabit
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
#include "messages.h"

#include "gsocket.h"

#include <iv.h>

typedef struct _ControlServerUnix
{
  ControlServer super;
  gchar *control_socket_name;
  gint control_socket;
  struct iv_fd control_listen;
} ControlServerUnix;

typedef struct _ControlConnectionUnix
{
  ControlConnection super;
  struct iv_fd control_io;
  gint fd;
  /* stdin, stdout, stderr as passed by syslog-ng-ctl */
  gint attached_fds[3];
} ControlConnectionUnix;

static gboolean
control_connection_unix_get_attached_fds(ControlConnection *s, gint *fds, gsize *num_fds)
{
  ControlConnectionUnix *self = (ControlConnectionUnix *)s;

  g_assert(*num_fds >= 3);
  memcpy(fds, self->attached_fds, sizeof(self->attached_fds));
  *num_fds = 3;
  return TRUE;
}

gint
control_connection_unix_write(ControlConnection *s, gpointer buffer, gsize size)
{
  ControlConnectionUnix *self = (ControlConnectionUnix *)s;
  return write(self->control_io.fd, buffer, size);
}

static gint
_extract_ancillary_data(ControlConnectionUnix *self, gint rc, struct msghdr *msg)
{
  if (G_UNLIKELY(msg->msg_flags & MSG_CTRUNC))
    {
      msg_warning_once("WARNING: recvmsg() on control socket returned truncated control data",
                       evt_tag_int("control_len", msg->msg_controllen));
      return -1;
    }

  for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg))
    {
      if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS)
        {
          gint header_len = CMSG_DATA(cmsg) - (unsigned char *) cmsg;
          gint fd_array_size = (cmsg->cmsg_len - header_len);

          if (fd_array_size != sizeof(self->attached_fds))
            {
              msg_warning_once("WARNING: invalid number of fds received on control socket",
                               evt_tag_int("fd_array_size", fd_array_size));
              return -1;
            }
          memcpy(&self->attached_fds, CMSG_DATA(cmsg), sizeof(self->attached_fds));
          break;
        }
    }

  return rc;
}

gint
control_connection_unix_read(ControlConnection *s, gpointer buffer, gsize size)
{
  ControlConnectionUnix *self = (ControlConnectionUnix *)s;
  gchar cmsg_buf[256];
  struct iovec iov[1] =
  {
    { .iov_base = buffer, .iov_len = size },
  };
  struct msghdr msg =
  {
    .msg_iov = iov,
    .msg_iovlen = G_N_ELEMENTS(iov),
    .msg_control = cmsg_buf,
    .msg_controllen = sizeof(cmsg_buf),
  };
  gint rc = recvmsg(self->control_io.fd, &msg, 0);
  if (rc < 0)
    return rc;

  return _extract_ancillary_data(self, rc, &msg);
}

static void
control_connection_unix_start_watches(ControlConnection *s)
{
  ControlConnectionUnix *self = (ControlConnectionUnix *)s;
  IV_FD_INIT(&self->control_io);
  self->control_io.cookie = self;
  self->control_io.fd = self->fd;
  iv_fd_register(&self->control_io);

  control_connection_update_watches(s);
}

static void
control_connection_unix_stop_watches(ControlConnection *s)
{
  ControlConnectionUnix *self = (ControlConnectionUnix *)s;
  iv_fd_unregister(&self->control_io);
}

static gboolean
_pending_output(ControlConnection *s)
{
  g_mutex_lock(&s->response_batches_lock);
  gboolean result = (s->output_buffer && s->output_buffer->len > s->pos) ||
                    !g_queue_is_empty(s->response_batches);
  g_mutex_unlock(&s->response_batches_lock);
  return result;
}

static void
control_connection_unix_update_watches(ControlConnection *s)
{
  ControlConnectionUnix *self = (ControlConnectionUnix *)s;

  if (s->waiting_for_output)
    {
      iv_fd_set_handler_out(&self->control_io, NULL);
      iv_fd_set_handler_in(&self->control_io, NULL);
    }
  else if (_pending_output(s))
    {
      iv_fd_set_handler_out(&self->control_io, s->handle_output);
      iv_fd_set_handler_in(&self->control_io, NULL);
    }
  else
    {
      iv_fd_set_handler_out(&self->control_io, NULL);
      iv_fd_set_handler_in(&self->control_io, s->handle_input);
    }
}

void
control_connection_unix_free(ControlConnection *s)
{
  ControlConnectionUnix *self = (ControlConnectionUnix *)s;
  close(self->control_io.fd);
  for (gint i = 0; i < G_N_ELEMENTS(self->attached_fds); i++)
    {
      if (self->attached_fds[i] >= 0)
        close(self->attached_fds[i]);
    }
}

ControlConnection *
control_connection_unix_new(ControlServer *server, gint sock)
{
  ControlConnectionUnix *self = g_new0(ControlConnectionUnix, 1);

  control_connection_init_instance(&self->super, server);
  self->fd = sock;
  self->super.free_fn = control_connection_unix_free;
  self->super.read = control_connection_unix_read;
  self->super.write = control_connection_unix_write;
  self->super.events.start_watches = control_connection_unix_start_watches;
  self->super.events.update_watches = control_connection_unix_update_watches;
  self->super.events.stop_watches = control_connection_unix_stop_watches;
  self->super.get_attached_fds = control_connection_unix_get_attached_fds;

  for (gint i = 0; i < G_N_ELEMENTS(self->attached_fds); i++)
    {
      self->attached_fds[i] = -1;
    }

  return &self->super;
}

static void
_control_socket_accept(void *cookie)
{
  ControlServerUnix *self = (ControlServerUnix *)cookie;
  gint conn_socket;
  GSockAddr *peer_addr;
  GIOStatus status;
  ControlConnection *cc;

  if (self->control_socket == -1)
    return;
  status = g_accept(self->control_socket, &conn_socket, &peer_addr);
  if (status != G_IO_STATUS_NORMAL)
    {
      msg_error("Error accepting control socket connection",
                evt_tag_error("error"));
      goto error;
    }

  cc = control_connection_unix_new(&self->super, conn_socket);

  /* NOTE: with the call below, the reference to the control connection (cc)
   * will be taken over by the various I/O callbacks, Those will return to
   * us (ControlServer) via the control_server_connection_closed() function,
   * which in turn will properly dispose the ControlConnection instance.
   */

  control_connection_start_watches(cc);
  g_sockaddr_unref(peer_addr);
error:
  ;
}

static gboolean
control_server_unix_start(ControlServer *s)
{
  ControlServerUnix *self = (ControlServerUnix *)s;
  GSockAddr *saddr;

  saddr = g_sockaddr_unix_new(self->control_socket_name);
  self->control_socket = socket(PF_UNIX, SOCK_STREAM, 0);
  if (self->control_socket == -1)
    {
      msg_error("Error opening control socket, external controls will not be available",
                evt_tag_str("socket", self->control_socket_name));
      goto error;
    }
  if (g_bind(self->control_socket, saddr) != G_IO_STATUS_NORMAL)
    {
      msg_error("Error opening control socket, bind() failed",
                evt_tag_str("socket", self->control_socket_name),
                evt_tag_error("error"));
      goto error;
    }
  if (listen(self->control_socket, 255) < 0)
    {
      msg_error("Error opening control socket, listen() failed",
                evt_tag_str("socket", self->control_socket_name),
                evt_tag_error("error"));
      goto error;
    }

  self->control_listen.fd = self->control_socket;
  self->control_listen.cookie = self;
  iv_fd_register(&self->control_listen);
  iv_fd_set_handler_in(&self->control_listen, _control_socket_accept);

  g_sockaddr_unref(saddr);
  return control_server_start_method(s);
error:
  if (self->control_socket != -1)
    {
      close(self->control_socket);
      self->control_socket = -1;
    }
  g_sockaddr_unref(saddr);
  return FALSE;
}

static void
control_server_unix_stop(ControlServer *s)
{
  ControlServerUnix *self = (ControlServerUnix *)s;

  if (iv_fd_registered(&self->control_listen))
    {
      iv_fd_unregister(&self->control_listen);
    }
  if (self->control_socket != -1)
    {
      close(self->control_socket);
    }
  control_server_stop_method(s);
}

void
control_server_unix_free(ControlServer *s)
{
  ControlServerUnix *self = (ControlServerUnix *)s;

  g_assert(!iv_fd_registered(&self->control_listen));
  g_free(self->control_socket_name);
  control_server_free_method(s);
}

ControlServer *
control_server_unix_new(const gchar *path)
{
  ControlServerUnix *self = g_new(ControlServerUnix, 1);

  control_server_init_instance(&self->super);
  self->super.start = control_server_unix_start;
  self->super.stop = control_server_unix_stop;
  self->super.free_fn = control_server_unix_free;
  self->control_socket_name = g_strdup(path);
  IV_FD_INIT(&self->control_listen);
  return &self->super;
}
