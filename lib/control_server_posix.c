#include "syslog-ng.h"
#include "messages.h"
#include "gsocket.h"
#include "control_server.h"
#include "misc.h"
#include "iv.h"
#include <string.h>

typedef struct _ControlServerPosix {
  ControlServer super;
  gint control_socket;
  struct iv_fd control_listen;
} ControlServerPosix;

typedef struct _ControlConnectionPosix
{
  ControlConnection super;
  struct iv_fd control_io;
  gint fd;
} ControlConnectionPosix;

gint
control_connection_posix_write(ControlConnection *s, gpointer buffer, gsize size)
{
  ControlConnectionPosix *self = (ControlConnectionPosix *)s;
  return write(self->control_io.fd, buffer, size);
}

gint
control_connection_posix_read(ControlConnection *s, gpointer buffer, gsize size)
{
  ControlConnectionPosix *self = (ControlConnectionPosix *)s;
  return read(self->control_io.fd, buffer, size);
}

void
control_connection_start_watches(ControlConnection *s)
{
  ControlConnectionPosix *self = (ControlConnectionPosix *)s;
  IV_FD_INIT(&self->control_io);
  self->control_io.cookie = self;
  self->control_io.fd = self->fd;
  iv_fd_register(&self->control_io);

  control_connection_update_watches(s);
}

void
control_connection_stop_watches(ControlConnection *s)
{
  ControlConnectionPosix *self = (ControlConnectionPosix *)s;
  iv_fd_unregister(&self->control_io);
}

void
control_connection_update_watches(ControlConnection *s)
{
  ControlConnectionPosix *self = (ControlConnectionPosix *)s;
  if (s->output_buffer->len > s->pos)
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
control_connection_posix_free(ControlConnection *s)
{
  ControlConnectionPosix *self = (ControlConnectionPosix *)s;
  close(self->control_io.fd);
}

ControlConnection *
control_connection_new(ControlServer *server, gint sock)
{
  ControlConnectionPosix *self = g_new0(ControlConnectionPosix, 1);

  control_connection_init_instance(&self->super, server);
  self->fd = sock;
  self->super.free_fn = control_connection_posix_free;
  self->super.read = control_connection_posix_read;
  self->super.write = control_connection_posix_write;

  control_connection_start_watches(&self->super);
  return &self->super;
}

static void
control_socket_accept(void *cookie)
{
  ControlServerPosix *self = (ControlServerPosix *)cookie;
  gint conn_socket;
  GSockAddr *peer_addr;
  GIOStatus status;

  if (self->control_socket == -1)
    return;
  status = g_accept(self->control_socket, &conn_socket, &peer_addr);
  if (status != G_IO_STATUS_NORMAL)
    {
      msg_error("Error accepting control socket connection",
                evt_tag_errno("error", errno),
                NULL);
      goto error;
    }
  /* NOTE: the connection will free itself if the peer terminates */
  control_connection_new(&self->super, conn_socket);
  g_sockaddr_unref(peer_addr);
 error:
  ;
}

void
control_server_start(ControlServer *s)
{
  ControlServerPosix *self = (ControlServerPosix *)s;
  GSockAddr *saddr;

  saddr = g_sockaddr_unix_new(self->super.control_socket_name);
  self->control_socket = socket(PF_UNIX, SOCK_STREAM, 0);
  if (self->control_socket == -1)
    {
      msg_error("Error opening control socket, external controls will not be available",
               evt_tag_str("socket", self->super.control_socket_name),
               NULL);
      return;
    }
  if (g_bind(self->control_socket, saddr) != G_IO_STATUS_NORMAL)
    {
      msg_error("Error opening control socket, bind() failed",
               evt_tag_str("socket", self->super.control_socket_name),
               evt_tag_errno("error", errno),
               NULL);
      goto error;
    }
  if (listen(self->control_socket, 255) < 0)
    {
      msg_error("Error opening control socket, listen() failed",
               evt_tag_str("socket", self->super.control_socket_name),
               evt_tag_errno("error", errno),
               NULL);
      goto error;
    }

  self->control_listen.fd = self->control_socket;
  self->control_listen.cookie = self;
  iv_fd_register(&self->control_listen);
  iv_fd_set_handler_in(&self->control_listen, control_socket_accept);

  g_sockaddr_unref(saddr);
  return;
 error:
  if (self->control_socket != -1)
    {
      close(self->control_socket);
      self->control_socket = -1;
    }
  g_sockaddr_unref(saddr);
  return;
}

void
control_server_posix_free(ControlServer *s)
{
  ControlServerPosix *self = (ControlServerPosix *)s;
  if (iv_fd_registered(&self->control_listen))
    {
      iv_fd_unregister(&self->control_listen);
    }
  if (self->control_socket != -1)
    {
      close(self->control_socket);
    }
}

ControlServer *
control_server_new(const gchar *path, Commands *commands)
{
  ControlServerPosix *self = g_new(ControlServerPosix,1);

  control_server_init_instance(&self->super, path, commands);
  IV_FD_INIT(&self->control_listen);
  self->super.free_fn = control_server_posix_free;
  return &self->super;
}
