#include "syslog-ng.h"
#include "messages.h"
#include "gsocket.h"
#include "control_server.h"
#include "misc.h"
#include "iv.h"
#include <string.h>

struct _ControlServer {
  gint control_socket;
  struct iv_fd control_listen;
  gchar *control_socket_name;
  GHashTable *command_list;
};

typedef struct _ControlConnection
{
  struct iv_fd control_io;
  GString *input_buffer;
  GString *output_buffer;
  gsize pos;
  ControlServer *server;
} ControlConnection;

COMMAND_HANDLER control_server_get_command_handler(ControlServer *self, gchar *cmd);

static void control_connection_update_watches(ControlConnection *self);
static void control_connection_stop_watches(ControlConnection *self);
void control_connection_free(ControlConnection *self);

static void
control_connection_send_reply(ControlConnection *self, GString *reply)
{
  g_string_assign(self->output_buffer, reply->str);
  g_string_free(reply, TRUE);

  self->pos = 0;

  if (self->output_buffer->str[self->output_buffer->len - 1] != '\n')
    g_string_append_c(self->output_buffer, '\n');
  g_string_append(self->output_buffer, ".\n");

  control_connection_update_watches(self);
}

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
control_connection_io_input(void *s)
{
  ControlConnection *self = (ControlConnection *) s;
  GString *command = NULL;
  GString *reply = NULL;
  gchar *nl;
  gchar *pos;
  gint rc;
  gint orig_len;
  COMMAND_HANDLER command_handler = NULL;

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

  pos = strchr(command->str, ' ');
  if (pos)
    *pos = '\0';
  command_handler = control_server_get_command_handler(self->server, command->str);
  if (pos)
    *pos = ' ';

  if (command_handler)
    {
      reply = command_handler(command);
      control_connection_send_reply(self, reply);
    }
  else
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

static void
control_connection_new(ControlServer *server, gint sock)
{
  ControlConnection *self = g_new0(ControlConnection, 1);

  self->output_buffer = g_string_sized_new(256);
  self->input_buffer = g_string_sized_new(128);
  self->server = server;

  control_connection_start_watches(self, sock);
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
control_socket_accept(void *cookie)
{
  ControlServer *self = (ControlServer *)cookie;
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
  control_connection_new(self, conn_socket);
  g_sockaddr_unref(peer_addr);
 error:
  ;
}

ControlServer *
control_server_new(const gchar *path)
{
  ControlServer *self = g_new(ControlServer,1);

  IV_FD_INIT(&self->control_listen);
  self->control_socket = -1;
  self->control_socket_name = g_strdup(path);
  self->command_list = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,NULL);
  return self;
}

gboolean
control_server_start(ControlServer *self)
{
  GSockAddr *saddr;

  saddr = g_sockaddr_unix_new(self->control_socket_name);
  self->control_socket = socket(PF_UNIX, SOCK_STREAM, 0);
  if (self->control_socket == -1)
    {
      msg_error("Error opening control socket, external controls will not be available",
               evt_tag_str("socket", self->control_socket_name),
               NULL);
      return FALSE;
    }
  if (g_bind(self->control_socket, saddr) != G_IO_STATUS_NORMAL)
    {
      msg_error("Error opening control socket, bind() failed",
               evt_tag_str("socket", self->control_socket_name),
               evt_tag_errno("error", errno),
               NULL);
      goto error;
    }
  if (listen(self->control_socket, 255) < 0)
    {
      msg_error("Error opening control socket, listen() failed",
               evt_tag_str("socket", self->control_socket_name),
               evt_tag_errno("error", errno),
               NULL);
      goto error;
    }

  self->control_listen.fd = self->control_socket;
  self->control_listen.cookie = self;
  iv_fd_register(&self->control_listen);
  iv_fd_set_handler_in(&self->control_listen, control_socket_accept);

  g_sockaddr_unref(saddr);
  return TRUE;
 error:
  if (self->control_socket != -1)
    {
      close(self->control_socket);
      self->control_socket = -1;
    }
  g_sockaddr_unref(saddr);
  return FALSE;
}

void
control_server_register_command_handler(ControlServer *self,const gchar *cmd, COMMAND_HANDLER handler)
{
  g_hash_table_insert(self->command_list, g_strdup(cmd), handler);
}

void
control_server_free(ControlServer *self)
{
  g_free(self->control_socket_name);
  if (iv_fd_registered(&self->control_listen))
    {
      iv_fd_unregister(&self->control_listen);
    }
  if (self->control_socket != -1)
    {
      close(self->control_socket);
    }
  g_hash_table_destroy(self->command_list);
  g_free(self);
}

COMMAND_HANDLER
control_server_get_command_handler(ControlServer *self, gchar *cmd)
{
  return (COMMAND_HANDLER)g_hash_table_lookup(self->command_list, cmd);
}
