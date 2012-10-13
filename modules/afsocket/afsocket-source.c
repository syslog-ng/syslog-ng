/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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

#include "afsocket-source.h"
#include "messages.h"
#include "misc.h"
#if BUILD_WITH_SSL
#include "tlstransport.h"
#endif
#include "gsocket.h"
#include "stats.h"
#include "mainloop.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#if ENABLE_TCP_WRAPPER
#include <tcpd.h>
int allow_severity = 0;
int deny_severity = 0;
#endif



typedef struct _AFSocketSourceConnection
{
  LogPipe super;
  struct _AFSocketSourceDriver *owner;
  LogPipe *reader;
  int sock;
  GSockAddr *peer_addr;
} AFSocketSourceConnection;

static void afsocket_sd_close_connection(AFSocketSourceDriver *self, AFSocketSourceConnection *sc);

static gint
afsocket_sc_stats_source(AFSocketSourceConnection *self)
{
  gint source;

  if ((self->owner->flags & AFSOCKET_SYSLOG_PROTOCOL) == 0)
    {
      switch (self->owner->bind_addr->sa.sa_family)
        {
        case AF_UNIX:
          source = (self->owner->sock_type == SOCK_STREAM) ? SCS_UNIX_STREAM : SCS_UNIX_DGRAM;
          break;
        case AF_INET:
          source = (self->owner->sock_type == SOCK_STREAM) ? SCS_TCP : SCS_UDP;
          break;
#if ENABLE_IPV6
        case AF_INET6:
          source = (self->owner->sock_type == SOCK_STREAM) ? SCS_TCP6 : SCS_UDP6;
          break;
#endif
        default:
          g_assert_not_reached();
          break;
        }
    }
  else
    {
      source = SCS_SYSLOG;
    }
  return source;
}

static gchar *
afsocket_sc_stats_instance(AFSocketSourceConnection *self)
{
  static gchar buf[256];

  if (!self->peer_addr)
    {
      if (self->owner->bind_addr)
        {
          g_sockaddr_format(self->owner->bind_addr, buf, sizeof(buf), GSA_ADDRESS_ONLY);
          return buf;
        }
      else
        return NULL;
    }
  if ((self->owner->flags & AFSOCKET_SYSLOG_PROTOCOL) == 0)
    {
      g_sockaddr_format(self->peer_addr, buf, sizeof(buf), GSA_ADDRESS_ONLY);
    }
  else
    {
      gchar peer_addr[MAX_SOCKADDR_STRING];

      g_sockaddr_format(self->peer_addr, peer_addr, sizeof(peer_addr), GSA_ADDRESS_ONLY);
      g_snprintf(buf, sizeof(buf), "%s,%s", self->owner->transport, peer_addr);
    }
  return buf;
}

static gboolean
afsocket_sc_init(LogPipe *s)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;
  LogTransport *transport;
  LogProtoServer *proto;
  LogProtoServerFactory *proto_factory;
  GlobalConfig *cfg = log_pipe_get_config(&self->owner->super.super.super);

  if (!self->reader)
    {
#if BUILD_WITH_SSL
      if (self->owner->tls_context)
        {
          TLSSession *tls_session = tls_context_setup_session(self->owner->tls_context);
          if (!tls_session)
            return FALSE;
          transport = log_transport_tls_new(tls_session, self->sock);
        }
      else
#endif
      if (self->owner->sock_type == SOCK_DGRAM)
        transport = log_transport_dgram_socket_new(self->sock);
      else
        transport = log_transport_stream_socket_new(self->sock);

      proto_factory = log_proto_server_get_factory(cfg, self->owner->logproto_name);
      proto = log_proto_server_factory_construct(proto_factory, transport, &self->owner->reader_options.proto_options.super);

      self->reader = log_reader_new(proto);
    }
  log_reader_set_options(self->reader, s, &self->owner->reader_options, 1, afsocket_sc_stats_source(self), self->owner->super.super.id, afsocket_sc_stats_instance(self));
  log_reader_set_peer_addr(self->reader, self->peer_addr);
  log_pipe_append(self->reader, s);
  if (log_pipe_init(self->reader, NULL))
    {
      return TRUE;
    }
  else
    {
      log_pipe_unref(self->reader);
      self->reader = NULL;
    }
  return FALSE;
}

static gboolean
afsocket_sc_deinit(LogPipe *s)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;

  log_pipe_unref(&self->owner->super.super.super);
  self->owner = NULL;

  log_pipe_deinit(self->reader);
  return TRUE;
}

static void
afsocket_sc_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;

  switch (notify_code)
    {
    case NC_CLOSE:
    case NC_READ_ERROR:
      {
        if (self->owner->sock_type == SOCK_STREAM)
          afsocket_sd_close_connection(self->owner, self);
        break;
      }
    }
}

static void
afsocket_sc_set_owner(AFSocketSourceConnection *self, AFSocketSourceDriver *owner)
{
  if (self->owner)
    {
      log_pipe_unref(&self->owner->super.super.super);
    }
  self->owner = owner;
  log_pipe_ref(&owner->super.super.super);

  log_pipe_append(&self->super, &owner->super.super.super);
}


/*
  This should be called by log_reader_free -> log_pipe_unref
  because this is the control pipe of the reader
*/
static void
afsocket_sc_free(LogPipe *s)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;
  g_sockaddr_unref(self->peer_addr);
  log_pipe_free_method(s);
}

AFSocketSourceConnection *
afsocket_sc_new(AFSocketSourceDriver *owner, GSockAddr *peer_addr, int fd)
{
  AFSocketSourceConnection *self = g_new0(AFSocketSourceConnection, 1);

  log_pipe_init_instance(&self->super);
  self->super.init = afsocket_sc_init;
  self->super.deinit = afsocket_sc_deinit;
  self->super.notify = afsocket_sc_notify;
  self->super.free_fn = afsocket_sc_free;
  log_pipe_ref(&owner->super.super.super);
  self->owner = owner;


  self->peer_addr = g_sockaddr_ref(peer_addr);
  self->sock = fd;
  return self;
}

void
afsocket_sd_set_transport(LogDriver *s, const gchar *transport)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;

  if (self->transport)
    g_free(self->transport);
  self->transport = g_strdup(transport);
}

void
afsocket_sd_add_connection(AFSocketSourceDriver *self, AFSocketSourceConnection *connection)
{
  self->connections = g_list_prepend(self->connections,connection);
}

static void
afsocket_sd_kill_connection(AFSocketSourceConnection *connection)
{
  log_pipe_deinit(&connection->super);

  /* Remove the circular reference between the connection and its
   * reader (through the connection->reader and reader->control
   * pointers these have a circular references).
   */
  log_pipe_unref(connection->reader);
  connection->reader = NULL;

  log_pipe_unref(&connection->super);
}

static void
afsocket_sd_kill_connection_list(GList *list)
{
  GList *l, *next;

  /* NOTE: the list may contain a list of
   *   - deinitialized AFSocketSourceConnection instances (in case the persist-config list is
   *     freed), or
   *    - initialized AFSocketSourceConnection instances (in case keep-alive is turned off)
   */
  for (l = list; l; l = next)
    {
      AFSocketSourceConnection *connection = (AFSocketSourceConnection *) l->data;

      next = l->next;

      if (connection->owner)
        connection->owner->connections = g_list_remove(connection->owner->connections, connection);
      afsocket_sd_kill_connection(connection);
    }
}

void
afsocket_sd_set_keep_alive(LogDriver *s, gint enable)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;

  if (enable)
    self->flags |= AFSOCKET_KEEP_ALIVE;
  else
    self->flags &= ~AFSOCKET_KEEP_ALIVE;
}

void
afsocket_sd_set_max_connections(LogDriver *s, gint max_connections)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;

  self->max_connections = max_connections;
}

#if BUILD_WITH_SSL
void
afsocket_sd_set_tls_context(LogDriver *s, TLSContext *tls_context)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;

  self->tls_context = tls_context;
}
#endif

static inline gchar *
afsocket_sd_format_persist_name(AFSocketSourceDriver *self, gboolean listener_name)
{
  static gchar persist_name[128];
  gchar buf[64];

  g_snprintf(persist_name, sizeof(persist_name),
             listener_name ? "afsocket_sd_listen_fd(%s,%s)" : "afsocket_sd_connections(%s,%s)",
             (self->sock_type == SOCK_STREAM) ? "stream" : "dgram",
             g_sockaddr_format(self->bind_addr, buf, sizeof(buf), GSA_FULL));
  return persist_name;
}

gboolean
afsocket_sd_process_connection(AFSocketSourceDriver *self, GSockAddr *client_addr, GSockAddr *local_addr, gint fd)
{
  gchar buf[MAX_SOCKADDR_STRING], buf2[MAX_SOCKADDR_STRING];
#if ENABLE_TCP_WRAPPER
  if (client_addr && (client_addr->sa.sa_family == AF_INET
#if ENABLE_IPV6
                   || client_addr->sa.sa_family == AF_INET6
#endif
     ))
    {
      struct request_info req;

      request_init(&req, RQ_DAEMON, "syslog-ng", RQ_FILE, fd, 0);
      fromhost(&req);
      if (hosts_access(&req) == 0)
        {

          msg_error("Syslog connection rejected by tcpd",
                    evt_tag_str("client", g_sockaddr_format(client_addr, buf, sizeof(buf), GSA_FULL)),
                    evt_tag_str("local", g_sockaddr_format(local_addr, buf2, sizeof(buf2), GSA_FULL)),
                    NULL);
          return FALSE;
        }
    }

#endif

  if (self->num_connections >= self->max_connections)
    {
      msg_error("Number of allowed concurrent connections reached, rejecting connection",
                evt_tag_str("client", g_sockaddr_format(client_addr, buf, sizeof(buf), GSA_FULL)),
                evt_tag_str("local", g_sockaddr_format(local_addr, buf2, sizeof(buf2), GSA_FULL)),
                evt_tag_int("max", self->max_connections),
                NULL);
      return FALSE;
    }
  else
    {
      AFSocketSourceConnection *conn;

      conn = afsocket_sc_new(self, client_addr, fd);
      if (log_pipe_init(&conn->super, NULL))
        {
          afsocket_sd_add_connection(self,conn);
          self->num_connections++;
          log_pipe_append(&conn->super, &self->super.super.super);
        }
      else
        {
          log_pipe_unref(&conn->super);
          return FALSE;
        }
    }
  return TRUE;
}

#define MAX_ACCEPTS_AT_A_TIME 30

static void
afsocket_sd_accept(gpointer s)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  GSockAddr *peer_addr;
  gchar buf1[256], buf2[256];
  gint new_fd;
  gboolean res;
  int accepts = 0;

  while (accepts < MAX_ACCEPTS_AT_A_TIME)
    {
      GIOStatus status;

      status = g_accept(self->fd, &new_fd, &peer_addr);
      if (status == G_IO_STATUS_AGAIN)
        {
          /* no more connections to accept */
          break;
        }
      else if (status != G_IO_STATUS_NORMAL)
        {
          msg_error("Error accepting new connection",
                    evt_tag_errno(EVT_TAG_OSERROR, errno),
                    NULL);
          return;
        }
      if (self->setup_socket && !self->setup_socket(self, new_fd))
        {
          close(new_fd);
          return;
        }

      g_fd_set_nonblock(new_fd, TRUE);
      g_fd_set_cloexec(new_fd, TRUE);

      res = afsocket_sd_process_connection(self, peer_addr, self->bind_addr, new_fd);

      if (res)
        {
          if (peer_addr->sa.sa_family != AF_UNIX)
            msg_notice("Syslog connection accepted",
                        evt_tag_int("fd", new_fd),
                        evt_tag_str("client", g_sockaddr_format(peer_addr, buf1, sizeof(buf1), GSA_FULL)),
                        evt_tag_str("local", g_sockaddr_format(self->bind_addr, buf2, sizeof(buf2), GSA_FULL)),
                        NULL);
          else
            msg_verbose("Syslog connection accepted",
                        evt_tag_int("fd", new_fd),
                        evt_tag_str("client", g_sockaddr_format(peer_addr, buf1, sizeof(buf1), GSA_FULL)),
                        evt_tag_str("local", g_sockaddr_format(self->bind_addr, buf2, sizeof(buf2), GSA_FULL)),
                        NULL);
        }
      else
        {
          close(new_fd);
        }

      g_sockaddr_unref(peer_addr);
      accepts++;
    }
  return;
}

static void
afsocket_sd_close_connection(AFSocketSourceDriver *self, AFSocketSourceConnection *sc)
{
  gchar buf1[MAX_SOCKADDR_STRING], buf2[MAX_SOCKADDR_STRING];

  if (sc->peer_addr->sa.sa_family != AF_UNIX)
    msg_notice("Syslog connection closed",
               evt_tag_int("fd", sc->sock),
               evt_tag_str("client", g_sockaddr_format(sc->peer_addr, buf1, sizeof(buf1), GSA_FULL)),
               evt_tag_str("local", g_sockaddr_format(self->bind_addr, buf2, sizeof(buf2), GSA_FULL)),
               NULL);
  else
    msg_verbose("Syslog connection closed",
               evt_tag_int("fd", sc->sock),
               evt_tag_str("client", g_sockaddr_format(sc->peer_addr, buf1, sizeof(buf1), GSA_FULL)),
               evt_tag_str("local", g_sockaddr_format(self->bind_addr, buf2, sizeof(buf2), GSA_FULL)),
               NULL);
  log_pipe_deinit(&sc->super);
  self->connections = g_list_remove(self->connections, sc);
  afsocket_sd_kill_connection(sc);
  self->num_connections--;
}

static void
afsocket_sd_start_watches(AFSocketSourceDriver *self)
{
  IV_FD_INIT(&self->listen_fd);
  self->listen_fd.fd = self->fd;
  self->listen_fd.cookie = self;
  self->listen_fd.handler_in = afsocket_sd_accept;
  iv_fd_register(&self->listen_fd);
}

static void
afsocket_sd_stop_watches(AFSocketSourceDriver *self)
{
  if (iv_fd_registered (&self->listen_fd))
    iv_fd_unregister(&self->listen_fd);
}

gboolean
afsocket_sd_init(LogPipe *s)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  gint sock;
  gboolean res = FALSE;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_src_driver_init_method(s))
    return FALSE;

  if (!afsocket_sd_apply_transport(self))
    return FALSE;

  g_assert(self->transport);
  g_assert(self->bind_addr);

  if (self->sock_type == SOCK_STREAM && (self->flags & (AFSOCKET_WNDSIZE_INITED)) == 0)
    {
      /* distribute the window evenly between each of our possible
       * connections.  This is quite pessimistic and can result in very low
       * window sizes. Increase that but warn the user at the same time
       */

      self->reader_options.super.init_window_size /= self->max_connections;
      if (self->reader_options.super.init_window_size < 100)
        {
          msg_warning("WARNING: window sizing for tcp sources were changed in " VERSION_3_3 ", the configuration value was divided by the value of max-connections(). The result was too small, clamping to 100 entries. Ensure you have a proper log_fifo_size setting to avoid message loss.",
                      evt_tag_int("orig_log_iw_size", self->reader_options.super.init_window_size),
                      evt_tag_int("new_log_iw_size", 100),
                      evt_tag_int("min_log_fifo_size", 100 * self->max_connections),
                      NULL);
          self->reader_options.super.init_window_size = 100;
        }
      self->flags |= AFSOCKET_WNDSIZE_INITED;
    }
  log_reader_options_init(&self->reader_options, cfg, self->super.super.group);

  /* fetch persistent connections first */
  if ((self->flags & AFSOCKET_KEEP_ALIVE))
    {
      GList *p;

      self->connections = cfg_persist_config_fetch(cfg, afsocket_sd_format_persist_name(self, FALSE));

      self->num_connections = 0;
      for (p = self->connections; p; p = p->next)
        {
          afsocket_sc_set_owner((AFSocketSourceConnection *) p->data, self);
          log_pipe_init((LogPipe *) p->data, NULL);
          self->num_connections++;
        }
    }

  /* ok, we have connection list, check if we need to open a listener */
  sock = -1;
  if (self->sock_type == SOCK_STREAM)
    {
      if (self->flags & AFSOCKET_KEEP_ALIVE)
        {
          /* NOTE: this assumes that fd 0 will never be used for listening fds,
           * main.c opens fd 0 so this assumption can hold */
          sock = GPOINTER_TO_UINT(cfg_persist_config_fetch(cfg, afsocket_sd_format_persist_name(self, TRUE))) - 1;
        }

      if (sock == -1)
        {
          if (!afsocket_sd_acquire_socket(self, &sock))
            return self->super.super.optional;
          if (sock == -1 && !afsocket_open_socket(self->bind_addr, self->sock_type, self->sock_protocol, &sock))
            return self->super.super.optional;
        }

      /* set up listening source */
      if (listen(sock, self->listen_backlog) < 0)
        {
          msg_error("Error during listen()",
                    evt_tag_errno(EVT_TAG_OSERROR, errno),
                    NULL);
          close(sock);
          return FALSE;
        }

      if (self->setup_socket && !self->setup_socket(self, sock))
        {
          close(sock);
          return FALSE;
        }

      self->fd = sock;
      afsocket_sd_start_watches(self);
      res = TRUE;
    }
  else
    {
      if (!self->connections)
        {
          if (!afsocket_sd_acquire_socket(self, &sock))
            return self->super.super.optional;
          if (sock == -1 && !afsocket_open_socket(self->bind_addr, self->sock_type, self->sock_protocol, &sock))
            return self->super.super.optional;

          if (!self->setup_socket(self, sock))
            {
              close(sock);
              return FALSE;
            }
        }
      self->fd = -1;

      /* we either have self->connections != NULL, or sock contains a new fd */
      if (self->connections || afsocket_sd_process_connection(self, NULL, self->bind_addr, sock))
        res = TRUE;
    }
  return res;
}

static void
afsocket_sd_close_fd(gpointer value)
{
  gint fd = GPOINTER_TO_UINT(value) - 1;
  close(fd);
}

gboolean
afsocket_sd_deinit(LogPipe *s)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if ((self->flags & AFSOCKET_KEEP_ALIVE) == 0 || !cfg->persist)
    {
      afsocket_sd_kill_connection_list(self->connections);
    }
  else
    {
      GList *p;

      /* for SOCK_STREAM source drivers this is a list, for
       * SOCK_DGRAM this is a single connection */

      for (p = self->connections; p; p = p->next)
        {
          log_pipe_deinit((LogPipe *) p->data);
        }
      cfg_persist_config_add(cfg, afsocket_sd_format_persist_name(self, FALSE), self->connections, (GDestroyNotify) afsocket_sd_kill_connection_list, FALSE);
    }
  self->connections = NULL;

  if (self->sock_type == SOCK_STREAM)
    {
      afsocket_sd_stop_watches(self);
      if ((self->flags & AFSOCKET_KEEP_ALIVE) == 0)
        {
          msg_verbose("Closing listener fd",
                      evt_tag_int("fd", self->fd),
                      NULL);
          close(self->fd);
        }
      else
        {
          /* NOTE: the fd is incremented by one when added to persistent config
           * as persist config cannot store NULL */

          cfg_persist_config_add(cfg, afsocket_sd_format_persist_name(self, TRUE), GUINT_TO_POINTER(self->fd + 1), afsocket_sd_close_fd, FALSE);
        }
    }
  else if (self->sock_type == SOCK_DGRAM)
    {
      /* we don't need to close the listening fd here as we have a
       * single connection which will close it */

      ;
    }

  if (!log_src_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

static void
afsocket_sd_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  switch (notify_code)
    {
    case NC_CLOSE:
    case NC_READ_ERROR:
      {
        g_assert_not_reached();
        break;
      }
    }
}

static gboolean
afsocket_sd_setup_socket(AFSocketSourceDriver *self, gint fd)
{
  return afsocket_setup_socket(fd, self->sock_options_ptr, AFSOCKET_DIR_RECV);
}

void
afsocket_sd_free(LogPipe *s)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;

  log_reader_options_destroy(&self->reader_options);
  g_sockaddr_unref(self->bind_addr);
  self->bind_addr = NULL;
  g_free(self->transport);
#if BUILD_WITH_SSL
  if(self->tls_context)
    {
      tls_context_free(self->tls_context);
    }
#endif
  log_src_driver_free(s);
}

void
afsocket_sd_init_instance(AFSocketSourceDriver *self, SocketOptions *sock_options, gint family, gint sock_type, guint32 flags)
{
  log_src_driver_init_instance(&self->super);

  self->super.super.super.init = afsocket_sd_init;
  self->super.super.super.deinit = afsocket_sd_deinit;
  self->super.super.super.free_fn = afsocket_sd_free;
  /* NULL behaves as if log_pipe_forward_msg was specified */
  self->super.super.super.queue = NULL;
  self->super.super.super.notify = afsocket_sd_notify;
  self->sock_options_ptr = sock_options;
  self->setup_socket = afsocket_sd_setup_socket;
  self->address_family = family;
  self->max_connections = 10;
  self->listen_backlog = 255;
  self->sock_type = sock_type;
  self->flags = flags | AFSOCKET_KEEP_ALIVE;
  log_reader_options_defaults(&self->reader_options);
  if (self->sock_type == SOCK_STREAM)
    self->reader_options.super.init_window_size = 1000;

  if (self->flags & AFSOCKET_LOCAL)
    {
      static gboolean warned = FALSE;

      self->reader_options.parse_options.flags |= LP_LOCAL;
      if (cfg_is_config_version_older(configuration, 0x0302))
        {
          if (!warned)
            {
              msg_warning("WARNING: the expected message format is being changed for unix-domain transports to improve "
                          "syslogd compatibity with " VERSION_3_2 ". If you are using custom "
                          "applications which bypass the syslog() API, you might "
                          "need the 'expect-hostname' flag to get the old behaviour back", NULL);
              warned = TRUE;
            }
        }
      else
        {
          self->reader_options.parse_options.flags &= ~LP_EXPECT_HOSTNAME;
        }
    }
  if ((self->flags & AFSOCKET_SYSLOG_PROTOCOL))
    {
      self->reader_options.parse_options.flags |= LP_SYSLOG_PROTOCOL;
    }
}
