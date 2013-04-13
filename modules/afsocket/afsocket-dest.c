/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
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

#include "afsocket-dest.h"
#include "messages.h"
#include "misc.h"
#include "logwriter.h"
#if BUILD_WITH_SSL
#include "tlstransport.h"
#endif
#include "gsocket.h"
#include "stats.h"
#include "mainloop.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

void
afsocket_dd_set_transport(LogDriver *s, const gchar *transport)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  if (self->transport)
    g_free(self->transport);
  self->transport = g_strdup(transport);
}

#if BUILD_WITH_SSL
void
afsocket_dd_set_tls_context(LogDriver *s, TLSContext *tls_context)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  self->tls_context = tls_context;
}
#endif

void
afsocket_dd_set_keep_alive(LogDriver *s, gboolean enable)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  self->connections_kept_alive_accross_reloads = enable;
}


static gchar *
afsocket_dd_format_persist_name(AFSocketDestDriver *self, gboolean qfile)
{
  static gchar persist_name[128];

  g_snprintf(persist_name, sizeof(persist_name),
             qfile ? "afsocket_dd_qfile(%s,%s)" : "afsocket_dd_connection(%s,%s)",
             (self->sock_type == SOCK_STREAM) ? "stream" : "dgram",
             self->dest_name);
  return persist_name;
}


static gint
afsocket_dd_stats_source(AFSocketDestDriver *self)
{
  gint source = 0;

  /* FIXME: make this a overrideable function */
  if (!self->syslog_protocol)
    {
      switch (self->bind_addr->sa.sa_family)
        {
        case AF_UNIX:
          source = (self->sock_type == SOCK_STREAM) ? SCS_UNIX_STREAM : SCS_UNIX_DGRAM;
          break;
        case AF_INET:
          source = (self->sock_type == SOCK_STREAM) ? SCS_TCP : SCS_UDP;
          break;
#if ENABLE_IPV6
        case AF_INET6:
          source = (self->sock_type == SOCK_STREAM) ? SCS_TCP6 : SCS_UDP6;
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
afsocket_dd_stats_instance(AFSocketDestDriver *self)
{
  if (!self->syslog_protocol)
    {
      return self->dest_name;
    }
  else
    {
      static gchar buf[256];

      g_snprintf(buf, sizeof(buf), "%s,%s", self->transport, self->dest_name);
      return buf;
    }
}

#if BUILD_WITH_SSL
static gint
afsocket_dd_tls_verify_callback(gint ok, X509_STORE_CTX *ctx, gpointer user_data)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) user_data;

  if (ok && ctx->current_cert == ctx->cert && self->hostname && (self->tls_context->verify_mode & TVM_TRUSTED))
    {
      ok = tls_verify_certificate_name(ctx->cert, self->hostname);
    }

  return ok;
}
#endif

static gboolean afsocket_dd_connected(AFSocketDestDriver *self);
static void afsocket_dd_reconnect(AFSocketDestDriver *self);

static void
afsocket_dd_init_watches(AFSocketDestDriver *self)
{
  IV_FD_INIT(&self->connect_fd);
  self->connect_fd.cookie = self;
  self->connect_fd.handler_out = (void (*)(void *)) afsocket_dd_connected;

  IV_TIMER_INIT(&self->reconnect_timer);
  self->reconnect_timer.cookie = self;
  self->reconnect_timer.handler = (void (*)(void *)) afsocket_dd_reconnect;
}

static void
afsocket_dd_start_watches(AFSocketDestDriver *self)
{
  main_loop_assert_main_thread();

  self->connect_fd.fd = self->fd;
  iv_fd_register(&self->connect_fd);
}

static void
afsocket_dd_stop_watches(AFSocketDestDriver *self)
{
  main_loop_assert_main_thread();

  if (iv_fd_registered(&self->connect_fd))
    {
      iv_fd_unregister(&self->connect_fd);

      /* need to close the fd in this case as it wasn't established yet */
      msg_verbose("Closing connecting fd",
                  evt_tag_int("fd", self->fd),
                  NULL);
      close(self->fd);
    }
  if (iv_timer_registered(&self->reconnect_timer))
    iv_timer_unregister(&self->reconnect_timer);
}

static void
afsocket_dd_start_reconnect_timer(AFSocketDestDriver *self)
{
  main_loop_assert_main_thread();

  if (iv_timer_registered(&self->reconnect_timer))
    iv_timer_unregister(&self->reconnect_timer);
  iv_validate_now();

  self->reconnect_timer.expires = iv_now;
  timespec_add_msec(&self->reconnect_timer.expires, self->time_reopen * 1000);
  iv_timer_register(&self->reconnect_timer);
}

static gboolean
afsocket_dd_connected(AFSocketDestDriver *self)
{
  gchar buf1[256], buf2[256];
  int error = 0;
  socklen_t errorlen = sizeof(error);
  LogTransport *transport;
  LogProtoClient *proto;

  main_loop_assert_main_thread();

  if (iv_fd_registered(&self->connect_fd))
    iv_fd_unregister(&self->connect_fd);

  if (self->sock_type == SOCK_STREAM)
    {
      if (getsockopt(self->fd, SOL_SOCKET, SO_ERROR, &error, &errorlen) == -1)
        {
          msg_error("getsockopt(SOL_SOCKET, SO_ERROR) failed for connecting socket",
                    evt_tag_int("fd", self->fd),
                    evt_tag_str("server", g_sockaddr_format(self->dest_addr, buf2, sizeof(buf2), GSA_FULL)),
                    evt_tag_errno(EVT_TAG_OSERROR, errno),
                    evt_tag_int("time_reopen", self->time_reopen),
                    NULL);
          goto error_reconnect;
        }
      if (error)
        {
          msg_error("Syslog connection failed",
                    evt_tag_int("fd", self->fd),
                    evt_tag_str("server", g_sockaddr_format(self->dest_addr, buf2, sizeof(buf2), GSA_FULL)),
                    evt_tag_errno(EVT_TAG_OSERROR, error),
                    evt_tag_int("time_reopen", self->time_reopen),
                    NULL);
          goto error_reconnect;
        }
    }
  msg_notice("Syslog connection established",
              evt_tag_int("fd", self->fd),
              evt_tag_str("server", g_sockaddr_format(self->dest_addr, buf2, sizeof(buf2), GSA_FULL)),
              evt_tag_str("local", g_sockaddr_format(self->bind_addr, buf1, sizeof(buf1), GSA_FULL)),
              NULL);


#if BUILD_WITH_SSL
  if (self->tls_context)
    {
      TLSSession *tls_session;

      tls_session = tls_context_setup_session(self->tls_context);
      if (!tls_session)
        {
          goto error_reconnect;
        }

      tls_session_set_verify(tls_session, afsocket_dd_tls_verify_callback, self, NULL);
      transport = log_transport_tls_new(tls_session, self->fd);
    }
  else
#endif

  if (self->sock_type == SOCK_STREAM)
    transport = log_transport_stream_socket_new(self->fd);
  else
    transport = log_transport_dgram_socket_new(self->fd);

  proto = log_proto_client_factory_construct(self->proto_factory, transport, &self->writer_options.proto_options.super);

  log_writer_reopen(self->writer, proto);
  return TRUE;
 error_reconnect:
  close(self->fd);
  self->fd = -1;
  afsocket_dd_start_reconnect_timer(self);
  return FALSE;
}

static gboolean
afsocket_dd_start_connect(AFSocketDestDriver *self)
{
  int sock, rc;
  gchar buf1[MAX_SOCKADDR_STRING], buf2[MAX_SOCKADDR_STRING];

  main_loop_assert_main_thread();
  if (!afsocket_open_socket(self->bind_addr, self->sock_type, self->sock_protocol, &sock))
    {
      return FALSE;
    }

  if (self->setup_socket && !self->setup_socket(self, sock))
    {
      close(sock);
      return FALSE;
    }

  g_assert(self->dest_addr);

  rc = g_connect(sock, self->dest_addr);
  if (rc == G_IO_STATUS_NORMAL)
    {
      self->fd = sock;
      afsocket_dd_connected(self);
    }
  else if (rc == G_IO_STATUS_ERROR && errno == EINPROGRESS)
    {
      /* we must wait until connect succeeds */

      self->fd = sock;
      afsocket_dd_start_watches(self);
    }
  else
    {
      /* error establishing connection */
      msg_error("Connection failed",
                evt_tag_int("fd", sock),
                evt_tag_str("server", g_sockaddr_format(self->dest_addr, buf2, sizeof(buf2), GSA_FULL)),
                evt_tag_str("local", g_sockaddr_format(self->bind_addr, buf1, sizeof(buf1), GSA_FULL)),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      close(sock);
      return FALSE;
    }

  return TRUE;
}

static void
afsocket_dd_reconnect(AFSocketDestDriver *self)
{
  if (!afsocket_dd_start_connect(self))
    {
      msg_error("Initiating connection failed, reconnecting",
                evt_tag_int("time_reopen", self->time_reopen),
                NULL);
      afsocket_dd_start_reconnect_timer(self);
    }
}

gboolean
afsocket_dd_init(LogPipe *s)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_dest_driver_init_method(s))
    return FALSE;

  if (!afsocket_dd_apply_transport(self))
    return FALSE;

  self->proto_factory = log_proto_client_get_factory(cfg, self->logproto_name);
  if (!self->proto_factory)
    {
      msg_error("Unknown value specified in the transport() option, no such LogProto plugin found",
                evt_tag_str("transport", self->logproto_name),
                NULL);
      return FALSE;
    }

  /* these fields must be set up by apply_transport, so let's check if it indeed did */
  g_assert(self->transport);
  g_assert(self->bind_addr);
  g_assert(self->hostname);
  g_assert(self->dest_name);

  if (cfg)
    {
      self->time_reopen = cfg->time_reopen;
    }

  log_writer_options_init(&self->writer_options, cfg, 0);
  self->writer = cfg_persist_config_fetch(cfg, afsocket_dd_format_persist_name(self, FALSE));
  if (!self->writer)
    {
      /* NOTE: we open our writer with no fd, so we can send messages down there
       * even while the connection is not established */

      self->writer = log_writer_new(LW_FORMAT_PROTO |
#if BUILD_WITH_SSL
                                    (((self->sock_type == SOCK_STREAM) && !self->tls_context) ? LW_DETECT_EOF : 0) |
#else
                                    ((self->sock_type == SOCK_STREAM) ? LW_DETECT_EOF : 0) |
#endif
                                    (self->syslog_protocol ? LW_SYSLOG_PROTOCOL : 0));

    }
  log_writer_set_options((LogWriter *) self->writer, &self->super.super.super,
                         &self->writer_options,
                         STATS_LEVEL0,
                         afsocket_dd_stats_source(self),
                         self->super.super.id,
                         afsocket_dd_stats_instance(self));
  log_writer_set_queue(self->writer, log_dest_driver_acquire_queue(&self->super, afsocket_dd_format_persist_name(self, TRUE)));

  log_pipe_init(self->writer, NULL);
  log_pipe_append(&self->super.super.super, self->writer);

  if (!log_writer_opened((LogWriter *) self->writer))
    afsocket_dd_reconnect(self);
  return TRUE;
}

gboolean
afsocket_dd_deinit(LogPipe *s)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  afsocket_dd_stop_watches(self);

  if (self->writer)
    log_pipe_deinit(self->writer);

  if (self->connections_kept_alive_accross_reloads)
    {
      cfg_persist_config_add(cfg, afsocket_dd_format_persist_name(self, FALSE), self->writer, (GDestroyNotify) log_pipe_unref, FALSE);
      self->writer = NULL;
    }

  if (!log_dest_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

static void
afsocket_dd_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;
  gchar buf[MAX_SOCKADDR_STRING];

  switch (notify_code)
    {
    case NC_CLOSE:
    case NC_WRITE_ERROR:
      log_writer_reopen(self->writer, NULL);

      msg_notice("Syslog connection broken",
                 evt_tag_int("fd", self->fd),
                 evt_tag_str("server", g_sockaddr_format(self->dest_addr, buf, sizeof(buf), GSA_FULL)),
                 evt_tag_int("time_reopen", self->time_reopen),
                 NULL);
      afsocket_dd_start_reconnect_timer(self);
      break;
    }
}

static gboolean
afsocket_dd_setup_socket(AFSocketDestDriver *self, gint fd)
{
  return afsocket_setup_socket(fd, self->sock_options_ptr, AFSOCKET_DIR_SEND);
}

void
afsocket_dd_free(LogPipe *s)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  log_writer_options_destroy(&self->writer_options);
  g_sockaddr_unref(self->bind_addr);
  g_sockaddr_unref(self->dest_addr);
  log_pipe_unref(self->writer);
  g_free(self->hostname);
  g_free(self->dest_name);
  g_free(self->transport);
#if BUILD_WITH_SSL
  if(self->tls_context)
    {
      tls_context_free(self->tls_context);
    }
#endif
  log_dest_driver_free(s);
}

void
afsocket_dd_init_instance(AFSocketDestDriver *self, SocketOptions *sock_options, gint family, gint sock_type, const gchar *hostname)
{
  log_dest_driver_init_instance(&self->super);

  log_writer_options_defaults(&self->writer_options);
  self->super.super.super.init = afsocket_dd_init;
  self->super.super.super.deinit = afsocket_dd_deinit;
  /* NULL behaves as if log_msg_forward_msg was specified */
  self->super.super.super.queue = NULL;
  self->super.super.super.free_fn = afsocket_dd_free;
  self->super.super.super.notify = afsocket_dd_notify;
  self->setup_socket = afsocket_dd_setup_socket;
  self->sock_options_ptr = sock_options;
  self->sock_type = sock_type;
  self->address_family = family;
  self->connections_kept_alive_accross_reloads = TRUE;

  self->hostname = g_strdup(hostname);

  self->writer_options.mark_mode = MM_GLOBAL;
  afsocket_dd_init_watches(self);
}
