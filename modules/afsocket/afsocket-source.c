/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "afsocket-source.h"
#include "messages.h"
#include "fdhelpers.h"
#include "gsocket.h"
#include "stats/stats-registry.h"
#include "mainloop.h"
#include "poll-fd-events.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#if SYSLOG_NG_ENABLE_TCP_WRAPPER
#include <tcpd.h>
int allow_severity = 0;
int deny_severity = 0;
#endif

static const gsize DYNAMIC_WINDOW_TIMER_SECS = 1;
static const gsize DYNAMIC_WINDOW_REALLOC_EVERY_STATS_TICKS = 5;

typedef struct _AFSocketSourceConnection
{
  LogPipe super;
  struct _AFSocketSourceDriver *owner;
  LogReader *reader;
  int sock;
  GSockAddr *peer_addr;
} AFSocketSourceConnection;

static void afsocket_sd_close_connection(AFSocketSourceDriver *self, AFSocketSourceConnection *sc);

static gchar *
afsocket_sc_stats_instance(AFSocketSourceConnection *self)
{
  static gchar buf[256];
  gchar peer_addr[MAX_SOCKADDR_STRING];

  if (!self->peer_addr)
    {
      /* dgram connection, which means we have no peer, use the bind address */
      if (self->owner->bind_addr)
        {
          g_sockaddr_format(self->owner->bind_addr, buf, sizeof(buf), GSA_ADDRESS_ONLY);
          return buf;
        }
      else
        return NULL;
    }

  g_sockaddr_format(self->peer_addr, peer_addr, sizeof(peer_addr), GSA_ADDRESS_ONLY);
  g_snprintf(buf, sizeof(buf), "%s,%s", self->owner->transport_mapper->transport, peer_addr);
  return buf;
}

static LogTransport *
afsocket_sc_construct_transport(AFSocketSourceConnection *self, gint fd)
{
  return transport_mapper_construct_log_transport(self->owner->transport_mapper, fd);
}

static gboolean
afsocket_sc_init(LogPipe *s)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;
  LogTransport *transport;
  LogProtoServer *proto;

  if (!self->reader)
    {
      transport = afsocket_sc_construct_transport(self, self->sock);
      /* transport_mapper_inet_construct_log_transport() can return NULL on TLS errors */
      if (!transport)
        return FALSE;

      proto = log_proto_server_factory_construct(self->owner->proto_factory, transport,
                                                 &self->owner->reader_options.proto_options.super);
      if (!proto)
        {
          log_transport_free(transport);
          return FALSE;
        }

      self->reader = log_reader_new(s->cfg);
      log_reader_open(self->reader, proto, poll_fd_events_new(self->sock));
      log_reader_set_peer_addr(self->reader, self->peer_addr);
    }

  if (self->owner->dynamic_window_ctr)
    log_source_enable_dynamic_window((LogSource *) self->reader, self->owner->dynamic_window_ctr);

  log_reader_set_options(self->reader, &self->super,
                         &self->owner->reader_options,
                         self->owner->super.super.id,
                         afsocket_sc_stats_instance(self));

  log_pipe_append((LogPipe *) self->reader, s);
  if (log_pipe_init((LogPipe *) self->reader))
    {
      return TRUE;
    }
  else
    {
      log_pipe_unref((LogPipe *) self->reader);
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

  log_pipe_deinit((LogPipe *) self->reader);
  return TRUE;
}

static void
afsocket_sc_notify(LogPipe *s, gint notify_code, gpointer user_data)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;

  switch (notify_code)
    {
    case NC_CLOSE:
    case NC_READ_ERROR:
    {
      if (self->owner->transport_mapper->sock_type == SOCK_STREAM)
        afsocket_sd_close_connection(self->owner, self);
      break;
    }
    default:
      break;
    }
}

static void
afsocket_sc_set_owner(AFSocketSourceConnection *self, AFSocketSourceDriver *owner)
{
  GlobalConfig *cfg = log_pipe_get_config(&owner->super.super.super);

  if (self->owner)
    log_pipe_unref(&self->owner->super.super.super);

  log_pipe_ref(&owner->super.super.super);
  self->owner = owner;
  self->super.expr_node = owner->super.super.super.expr_node;

  log_pipe_set_config(&self->super, cfg);
  if (self->reader)
    log_pipe_set_config((LogPipe *) self->reader, cfg);

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
afsocket_sc_new(GSockAddr *peer_addr, int fd, GlobalConfig *cfg)
{
  AFSocketSourceConnection *self = g_new0(AFSocketSourceConnection, 1);

  log_pipe_init_instance(&self->super, cfg);
  self->super.init = afsocket_sc_init;
  self->super.deinit = afsocket_sc_deinit;
  self->super.notify = afsocket_sc_notify;
  self->super.free_fn = afsocket_sc_free;
  self->peer_addr = g_sockaddr_ref(peer_addr);
  self->sock = fd;
  return self;
}

void
afsocket_sd_add_connection(AFSocketSourceDriver *self, AFSocketSourceConnection *connection)
{
  self->connections = g_list_prepend(self->connections, connection);
}

static void
afsocket_sd_kill_connection(AFSocketSourceConnection *connection)
{
  log_pipe_deinit(&connection->super);

  /* Remove the circular reference between the connection and its
   * reader (through the connection->reader and reader->control
   * pointers these have a circular references).
   */
  log_pipe_unref((LogPipe *) connection->reader);
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

  self->connections_kept_alive_across_reloads = enable;
}

void
afsocket_sd_set_max_connections(LogDriver *s, gint max_connections)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;

  self->max_connections = max_connections;
}

void
afsocket_sd_set_listen_backlog(LogDriver *s, gint listen_backlog)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;

  self->listen_backlog = listen_backlog;
}

void
afsocket_sd_set_dynamic_window_size(LogDriver *s, gint dynamic_window_size)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;

  if (!self->dynamic_window_ctr)
    self->dynamic_window_ctr = dynamic_window_counter_new();

  dynamic_window_counter_set_iw_size(self->dynamic_window_ctr, dynamic_window_size);
}

static const gchar *
afsocket_sd_format_name(const LogPipe *s)
{
  const AFSocketSourceDriver *self = (const AFSocketSourceDriver *)s;
  static gchar persist_name[1024];

  if (s->persist_name)
    {
      g_snprintf(persist_name, sizeof(persist_name), "afsocket_sd.%s",
                 self->super.super.super.persist_name);
    }
  else
    {
      gchar buf[64];

      g_snprintf(persist_name, sizeof(persist_name), "afsocket_sd.(%s,%s)",
                 (self->transport_mapper->sock_type == SOCK_STREAM) ? "stream" : "dgram",
                 g_sockaddr_format(self->bind_addr, buf, sizeof(buf), GSA_FULL));
    }

  return persist_name;
}

static const gchar *
afsocket_sd_format_listener_name(const AFSocketSourceDriver *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name), "%s.listen_fd",
             afsocket_sd_format_name((const LogPipe *)self));

  return persist_name;
}

static const gchar *
afsocket_sd_format_connections_name(const AFSocketSourceDriver *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name), "%s.connections",
             afsocket_sd_format_name((const LogPipe *)self));

  return persist_name;
}

static gboolean
afsocket_sd_process_connection(AFSocketSourceDriver *self, GSockAddr *client_addr, GSockAddr *local_addr, gint fd)
{
  gchar buf[MAX_SOCKADDR_STRING], buf2[MAX_SOCKADDR_STRING];
#if SYSLOG_NG_ENABLE_TCP_WRAPPER
  if (client_addr && (client_addr->sa.sa_family == AF_INET
#if SYSLOG_NG_ENABLE_IPV6
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
                    evt_tag_str("local", g_sockaddr_format(local_addr, buf2, sizeof(buf2), GSA_FULL)));
          return FALSE;
        }
    }

#endif

  if (self->num_connections >= self->max_connections)
    {
      msg_error("Number of allowed concurrent connections reached, rejecting connection",
                evt_tag_str("client", g_sockaddr_format(client_addr, buf, sizeof(buf), GSA_FULL)),
                evt_tag_str("local", g_sockaddr_format(local_addr, buf2, sizeof(buf2), GSA_FULL)),
                evt_tag_int("max", self->max_connections));
      return FALSE;
    }
  else
    {
      AFSocketSourceConnection *conn;

      conn = afsocket_sc_new(client_addr, fd, self->super.super.super.cfg);
      afsocket_sc_set_owner(conn, self);
      if (log_pipe_init(&conn->super))
        {
          afsocket_sd_add_connection(self, conn);
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
                    evt_tag_error(EVT_TAG_OSERROR));
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
                       evt_tag_str("local", g_sockaddr_format(self->bind_addr, buf2, sizeof(buf2), GSA_FULL)));
          else
            msg_verbose("Syslog connection accepted",
                        evt_tag_int("fd", new_fd),
                        evt_tag_str("client", g_sockaddr_format(peer_addr, buf1, sizeof(buf1), GSA_FULL)),
                        evt_tag_str("local", g_sockaddr_format(self->bind_addr, buf2, sizeof(buf2), GSA_FULL)));
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
               evt_tag_str("local", g_sockaddr_format(self->bind_addr, buf2, sizeof(buf2), GSA_FULL)));
  else
    msg_verbose("Syslog connection closed",
                evt_tag_int("fd", sc->sock),
                evt_tag_str("client", g_sockaddr_format(sc->peer_addr, buf1, sizeof(buf1), GSA_FULL)),
                evt_tag_str("local", g_sockaddr_format(self->bind_addr, buf2, sizeof(buf2), GSA_FULL)));

  log_reader_close_proto(sc->reader);
  log_pipe_deinit(&sc->super);
  self->connections = g_list_remove(self->connections, sc);
  afsocket_sd_kill_connection(sc);
  self->num_connections--;
}

static void
_listen_fd_init(AFSocketSourceDriver *self)
{
  IV_FD_INIT(&self->listen_fd);
  self->listen_fd.fd = self->fd;
  self->listen_fd.cookie = self;
  self->listen_fd.handler_in = afsocket_sd_accept;
  iv_fd_register(&self->listen_fd);
}

static void
_listen_fd_deinit(AFSocketSourceDriver *self)
{
  if (iv_fd_registered (&self->listen_fd))
    iv_fd_unregister(&self->listen_fd);
}

static void
_dynamic_window_timer_start(AFSocketSourceDriver *self)
{
  iv_validate_now();
  self->dynamic_window_timer.expires = iv_now;
  self->dynamic_window_timer.expires.tv_sec += DYNAMIC_WINDOW_TIMER_SECS; //TODO: set from cfg
  iv_timer_register(&self->dynamic_window_timer);
}

static void
_on_dynamic_window_timer_elapsed(gpointer cookie)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *)cookie;
  for (GList *conn_it = self->connections; conn_it; conn_it = conn_it->next) //TODO: refactor
    {
      AFSocketSourceConnection *conn = (AFSocketSourceConnection *) conn_it->data;
      if (self->dynamic_window_timer_tick == DYNAMIC_WINDOW_REALLOC_EVERY_STATS_TICKS)
        {
          log_source_dynamic_window_realloc(&conn->reader->super);
        }
      else
        {
          log_source_dynamic_window_update_statistics(&conn->reader->super);
        }
    }
  if (self->dynamic_window_timer_tick == DYNAMIC_WINDOW_REALLOC_EVERY_STATS_TICKS) //TODO: refactor
    self->dynamic_window_timer_tick = 0;
  self->dynamic_window_timer_tick++;

  msg_trace("Dynamic window timer elapsed", evt_tag_int("tick", self->dynamic_window_timer_tick));
  _dynamic_window_timer_start(self);
}

static void
_dynamic_window_timer_init(AFSocketSourceDriver *self)
{
  IV_TIMER_INIT(&self->dynamic_window_timer);
  self->dynamic_window_timer.cookie = self;
  self->dynamic_window_timer.handler = _on_dynamic_window_timer_elapsed;
}

static void
_dynamic_window_timer_deinit(AFSocketSourceDriver *self)
{
  if (iv_timer_registered(&self->dynamic_window_timer))
    iv_timer_unregister(&self->dynamic_window_timer);
}

static void
afsocket_sd_start_watches(AFSocketSourceDriver *self)
{
  _listen_fd_init(self);
  _dynamic_window_timer_init(self);
}

static void
afsocket_sd_stop_watches(AFSocketSourceDriver *self)
{
  _listen_fd_deinit(self);
  _dynamic_window_timer_deinit(self);
}

static gboolean
afsocket_sd_setup_reader_options(AFSocketSourceDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  if (self->transport_mapper->sock_type == SOCK_STREAM && !self->window_size_initialized)
    {
      /* Distribute the window evenly between each of our possible
       * connections.  This is quite pessimistic and can result in very low
       * window sizes. Increase that but warn the user at the same time.
       */

      self->reader_options.super.init_window_size /= self->max_connections;
      if (self->reader_options.super.init_window_size < cfg->min_iw_size_per_reader)
        {
          msg_warning("WARNING: window sizing for tcp sources were changed in " VERSION_3_3
                      ", the configuration value was divided by the value of max-connections(). The result was too small, clamping to value of min_iw_size_per_reader. Ensure you have a proper log_fifo_size setting to avoid message loss.",
                      evt_tag_int("orig_log_iw_size", self->reader_options.super.init_window_size),
                      evt_tag_int("new_log_iw_size", cfg->min_iw_size_per_reader),
                      evt_tag_int("min_iw_size_per_reader", cfg->min_iw_size_per_reader),
                      evt_tag_int("min_log_fifo_size", cfg->min_iw_size_per_reader * self->max_connections));
          self->reader_options.super.init_window_size = cfg->min_iw_size_per_reader;
        }
      self->window_size_initialized = TRUE;
    }
  log_reader_options_init(&self->reader_options, cfg, self->super.super.group);
  return TRUE;
}

static gboolean
afsocket_sd_setup_transport(AFSocketSourceDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  if (!transport_mapper_apply_transport(self->transport_mapper, cfg))
    return FALSE;

  if (!self->proto_factory)
    self->proto_factory = log_proto_server_get_factory(&cfg->plugin_context, self->transport_mapper->logproto);

  if (!self->proto_factory)
    {
      msg_error("Unknown value specified in the transport() option, no such LogProto plugin found",
                evt_tag_str("transport", self->transport_mapper->logproto));
      return FALSE;
    }

  self->transport_mapper->create_multitransport = self->proto_factory->use_multitransport;

  afsocket_sd_setup_reader_options(self);
  return TRUE;
}

static gboolean
afsocket_sd_restore_kept_alive_connections(AFSocketSourceDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  /* fetch persistent connections first */
  if (self->connections_kept_alive_across_reloads)
    {
      GList *p = NULL;
      self->connections = cfg_persist_config_fetch(cfg, afsocket_sd_format_connections_name(self));

      self->num_connections = 0;
      for (p = self->connections; p; p = p->next)
        {
          afsocket_sc_set_owner((AFSocketSourceConnection *) p->data, self);
          if (log_pipe_init((LogPipe *) p->data))
            {
              self->num_connections++;
            }
          else
            {
              AFSocketSourceConnection *sc = (AFSocketSourceConnection *)p->data;

              self->connections = g_list_remove(self->connections, sc);
              afsocket_sd_kill_connection((AFSocketSourceConnection *)sc);
            }
        }
    }
  return TRUE;
}

static gboolean
_finalize_init(gpointer arg)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *)arg;
  /* set up listening source */
  if (listen(self->fd, self->listen_backlog) < 0)
    {
      msg_error("Error during listen()",
                evt_tag_error(EVT_TAG_OSERROR));
      close(self->fd);
      self->fd = -1;
      return FALSE;
    }

  afsocket_sd_start_watches(self);
  if (self->dynamic_window_ctr)
    _dynamic_window_timer_start(self);
  char buf[256];
  msg_info("Accepting connections",
           evt_tag_str("addr", g_sockaddr_format(self->bind_addr, buf, sizeof(buf), GSA_FULL)));
  return TRUE;
}

static gboolean
_sd_open_stream(AFSocketSourceDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);
  gint sock = -1;
  if (self->connections_kept_alive_across_reloads)
    {
      /* NOTE: this assumes that fd 0 will never be used for listening fds,
       * main.c opens fd 0 so this assumption can hold */
      sock = GPOINTER_TO_UINT(
               cfg_persist_config_fetch(cfg, afsocket_sd_format_listener_name(self))) -
             1;
    }

  if (sock == -1)
    {
      if (!afsocket_sd_acquire_socket(self, &sock))
        return self->super.super.optional;
      if (sock == -1
          && !transport_mapper_open_socket(self->transport_mapper, self->socket_options, self->bind_addr, AFSOCKET_DIR_RECV,
                                           &sock))
        return self->super.super.optional;
    }
  self->fd = sock;
  return transport_mapper_async_init(self->transport_mapper, _finalize_init, self);
}

static gboolean
_sd_open_dgram(AFSocketSourceDriver *self)
{
  gint sock = -1;
  if (!self->connections)
    {
      if (!afsocket_sd_acquire_socket(self, &sock))
        return self->super.super.optional;
      if (sock == -1
          && !transport_mapper_open_socket(self->transport_mapper, self->socket_options, self->bind_addr, AFSOCKET_DIR_RECV,
                                           &sock))
        return self->super.super.optional;
    }
  self->fd = -1;

  /* we either have self->connections != NULL, or sock contains a new fd */
  if (self->connections || afsocket_sd_process_connection(self, NULL, self->bind_addr, sock))
    return transport_mapper_init(self->transport_mapper);
  return FALSE;
}

static gboolean
afsocket_sd_open_listener(AFSocketSourceDriver *self)
{
  if (self->transport_mapper->sock_type == SOCK_STREAM)
    {
      return _sd_open_stream(self);
    }
  else
    {
      return _sd_open_dgram(self);
    }
}

static void
afsocket_sd_close_fd(gpointer value)
{
  gint fd = GPOINTER_TO_UINT(value) - 1;
  close(fd);
}

static void
afsocket_sd_save_connections(AFSocketSourceDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  if (!self->connections_kept_alive_across_reloads || !cfg->persist)
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
      cfg_persist_config_add(cfg, afsocket_sd_format_connections_name(self), self->connections,
                             (GDestroyNotify)afsocket_sd_kill_connection_list, FALSE);
    }
  self->connections = NULL;
}

static void
afsocket_sd_save_listener(AFSocketSourceDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  if (self->transport_mapper->sock_type == SOCK_STREAM)
    {
      afsocket_sd_stop_watches(self);
      if (!self->connections_kept_alive_across_reloads)
        {
          msg_verbose("Closing listener fd",
                      evt_tag_int("fd", self->fd));
          close(self->fd);
        }
      else
        {
          /* NOTE: the fd is incremented by one when added to persistent config
           * as persist config cannot store NULL */

          cfg_persist_config_add(cfg, afsocket_sd_format_listener_name(self),
                                 GUINT_TO_POINTER(self->fd + 1), afsocket_sd_close_fd, FALSE);
        }
    }
}


gboolean
afsocket_sd_setup_addresses_method(AFSocketSourceDriver *self)
{
  return TRUE;
}

gboolean
afsocket_sd_init_method(LogPipe *s)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;

  if (self->dynamic_window_ctr)
    dynamic_window_counter_init(self->dynamic_window_ctr);

  return log_src_driver_init_method(s) &&
         afsocket_sd_setup_transport(self) &&
         afsocket_sd_setup_addresses(self) &&
         afsocket_sd_restore_kept_alive_connections(self) &&
         afsocket_sd_open_listener(self);
}

gboolean
afsocket_sd_deinit_method(LogPipe *s)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;

  afsocket_sd_save_connections(self);
  afsocket_sd_save_listener(self);

  return log_src_driver_deinit_method(s);
}

static void
afsocket_sd_notify(LogPipe *s, gint notify_code, gpointer user_data)
{
  switch (notify_code)
    {
    case NC_CLOSE:
    case NC_READ_ERROR:
    {
      g_assert_not_reached();
      break;
    }
    default:
      break;
    }
}

void
afsocket_sd_free_method(LogPipe *s)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;

  log_reader_options_destroy(&self->reader_options);
  transport_mapper_free(self->transport_mapper);
  socket_options_free(self->socket_options);
  g_sockaddr_unref(self->bind_addr);
  self->bind_addr = NULL;
  dynamic_window_counter_free(self->dynamic_window_ctr);
  log_src_driver_free(s);
}

void
afsocket_sd_init_instance(AFSocketSourceDriver *self,
                          SocketOptions *socket_options,
                          TransportMapper *transport_mapper,
                          GlobalConfig *cfg)
{
  log_src_driver_init_instance(&self->super, cfg);

  self->super.super.super.init = afsocket_sd_init_method;
  self->super.super.super.deinit = afsocket_sd_deinit_method;
  self->super.super.super.free_fn = afsocket_sd_free_method;
  self->super.super.super.notify = afsocket_sd_notify;
  self->super.super.super.generate_persist_name = afsocket_sd_format_name;
  self->setup_addresses = afsocket_sd_setup_addresses_method;
  self->socket_options = socket_options;
  self->transport_mapper = transport_mapper;
  self->max_connections = 10;
  self->listen_backlog = 255;
  self->connections_kept_alive_across_reloads = TRUE;
  log_reader_options_defaults(&self->reader_options);
  self->reader_options.super.stats_level = STATS_LEVEL1;
  self->reader_options.super.stats_source = transport_mapper->stats_source;

  /* NOTE: this changes the initial window size from 100 to 1000. Reasons:
   * Starting with syslog-ng 3.3, window-size is distributed evenly between
   * _all_ possible connections to avoid starving.  With the defaults this
   * means that we get a window size of 10 messages log_iw_size(100) /
   * max_connections(10), but that is incredibly slow, thus bump this value here.
   */

  self->reader_options.super.init_window_size = 1000;
}
