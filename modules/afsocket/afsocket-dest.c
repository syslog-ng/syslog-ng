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

#include "afsocket-dest.h"
#include "messages.h"
#include "logwriter.h"
#include "gsocket.h"
#include "stats/stats-registry.h"
#include "mainloop.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef struct _ReloadStoreItem
{
  LogProtoClientFactory *proto_factory;
  LogWriter *writer;
} ReloadStoreItem;

static ReloadStoreItem *
_reload_store_item_new(AFSocketDestDriver *afsocket_dd)
{
  ReloadStoreItem *item = g_new(ReloadStoreItem, 1);
  item->proto_factory = afsocket_dd->proto_factory;
  item->writer = afsocket_dd->writer;
  return item;
}

static void
_reload_store_item_free(ReloadStoreItem *self)
{
  if (!self)
    return;

  if (self->writer)
    log_pipe_unref((LogPipe *) self->writer);

  g_free(self);
}

static LogWriter *
_reload_store_item_release_writer(ReloadStoreItem *self)
{
  LogWriter *writer = self->writer;
  self->writer = NULL;

  return writer;
}

static inline gboolean
_is_protocol_compatible_with_writer_after_reload(AFSocketDestDriver *self, ReloadStoreItem *item)
{
  return (self->proto_factory->construct == item->proto_factory->construct);
}

void
afsocket_dd_set_keep_alive(LogDriver *s, gboolean enable)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  self->connections_kept_alive_across_reloads = enable;
}

static const gchar *_module_name = "afsocket_dd";

static const gchar *
_get_module_identifier(const AFSocketDestDriver *self)
{
  static gchar module_identifier[128];

  g_snprintf(module_identifier, sizeof(module_identifier), "%s,%s",
             (self->transport_mapper->sock_type == SOCK_STREAM) ? "stream" : "dgram",
             afsocket_dd_get_dest_name(self));

  return self->super.super.super.persist_name ? self->super.super.super.persist_name
         : module_identifier;
}

static const gchar *
afsocket_dd_format_name(const LogPipe *s)
{
  const AFSocketDestDriver *self = (const AFSocketDestDriver *)s;
  static gchar persist_name[1024];
  g_snprintf(persist_name, sizeof(persist_name), "%s.(%s)", _module_name,
             _get_module_identifier(self));

  return persist_name;
}

static const gchar *
afsocket_dd_format_qfile_name(const AFSocketDestDriver *self)
{
  static gchar persist_name[1024];
  g_snprintf(persist_name, sizeof(persist_name), "%s_qfile(%s)", _module_name,
             _get_module_identifier(self));

  return persist_name;
}

static const gchar *
afsocket_dd_format_connections_name(const AFSocketDestDriver *self)
{
  static gchar persist_name[1024];
  g_snprintf(persist_name, sizeof(persist_name), "%s_connections(%s)", _module_name,
             _get_module_identifier(self));

  return persist_name;
}

static gchar *
afsocket_dd_stats_instance(AFSocketDestDriver *self)
{
  static gchar buf[256];

  g_snprintf(buf, sizeof(buf), "%s,%s", self->transport_mapper->transport, afsocket_dd_get_dest_name(self));
  return buf;
}

static gboolean afsocket_dd_connected(AFSocketDestDriver *self);
static void afsocket_dd_reconnect(AFSocketDestDriver *self);
static void afsocket_dd_try_connect(AFSocketDestDriver *self);
static gboolean afsocket_dd_setup_connection(AFSocketDestDriver *self);

static void
afsocket_dd_init_watches(AFSocketDestDriver *self)
{
  IV_FD_INIT(&self->connect_fd);
  self->connect_fd.cookie = self;
  self->connect_fd.handler_out = (void (*)(void *)) afsocket_dd_connected;

  IV_TIMER_INIT(&self->reconnect_timer);
  self->reconnect_timer.cookie = self;
  /* Using reinit as a handler before establishing the first successful connection.
   * We'll change this to afsocket_dd_reconnect when the initialization of the
   * connection succeeds.*/
  self->reconnect_timer.handler = (void (*)(void *)) afsocket_dd_try_connect;
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
                  evt_tag_int("fd", self->fd));
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

static LogTransport *
afsocket_dd_construct_transport(AFSocketDestDriver *self, gint fd)
{
  return transport_mapper_construct_log_transport(self->transport_mapper, fd);
}

static gboolean
afsocket_dd_connected(AFSocketDestDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  gchar buf1[256], buf2[256];
  int error = 0;
  socklen_t errorlen = sizeof(error);
  LogTransport *transport;
  LogProtoClient *proto;

  main_loop_assert_main_thread();

  if (iv_fd_registered(&self->connect_fd))
    iv_fd_unregister(&self->connect_fd);

  if (self->transport_mapper->sock_type == SOCK_STREAM)
    {
      if (getsockopt(self->fd, SOL_SOCKET, SO_ERROR, &error, &errorlen) == -1)
        {
          msg_error("getsockopt(SOL_SOCKET, SO_ERROR) failed for connecting socket",
                    evt_tag_int("fd", self->fd),
                    evt_tag_str("server", g_sockaddr_format(self->dest_addr, buf2, sizeof(buf2), GSA_FULL)),
                    evt_tag_error(EVT_TAG_OSERROR),
                    evt_tag_int("time_reopen", self->time_reopen));
          goto error_reconnect;
        }
      if (error)
        {
          msg_error("Syslog connection failed",
                    evt_tag_int("fd", self->fd),
                    evt_tag_str("server", g_sockaddr_format(self->dest_addr, buf2, sizeof(buf2), GSA_FULL)),
                    evt_tag_errno(EVT_TAG_OSERROR, error),
                    evt_tag_int("time_reopen", self->time_reopen));
          goto error_reconnect;
        }
    }
  msg_notice("Syslog connection established",
             evt_tag_int("fd", self->fd),
             evt_tag_str("server", g_sockaddr_format(self->dest_addr, buf2, sizeof(buf2), GSA_FULL)),
             evt_tag_str("local", g_sockaddr_format(self->bind_addr, buf1, sizeof(buf1), GSA_FULL)));

  transport = afsocket_dd_construct_transport(self, self->fd);
  if (!transport)
    goto error_reconnect;

  proto = log_proto_client_factory_construct(self->proto_factory, transport, &self->writer_options.proto_options.super);

  log_proto_client_restart_with_state(proto, cfg->state, afsocket_dd_format_connections_name(self));
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

  g_assert(self->transport_mapper->transport);
  g_assert(self->bind_addr);

  if (!transport_mapper_open_socket(self->transport_mapper, self->socket_options, self->bind_addr, AFSOCKET_DIR_SEND,
                                    &sock))
    {
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
                evt_tag_error(EVT_TAG_OSERROR));
      close(sock);
      return FALSE;
    }

  return TRUE;
}

static void
_dd_reconnect(AFSocketDestDriver *self, gboolean request_setup_addr)
{
  if ((request_setup_addr && !afsocket_dd_setup_addresses(self)) || !afsocket_dd_start_connect(self))
    {
      msg_error("Initiating connection failed, reconnecting",
                evt_tag_int("time_reopen", self->time_reopen));
      afsocket_dd_start_reconnect_timer(self);
    }
}

static void
_dd_reconnect_with_setup_addresses(AFSocketDestDriver *self)
{
  _dd_reconnect(self, TRUE);
}

static void
_dd_reconnect_with_current_addresses(AFSocketDestDriver *self)
{
  _dd_reconnect(self, FALSE);
}

static void
afsocket_dd_reconnect(AFSocketDestDriver *self)
{
  _dd_reconnect_with_setup_addresses(self);
}

static void
afsocket_dd_try_connect(AFSocketDestDriver *self)
{
  if ((!afsocket_dd_setup_addresses(self)) || !afsocket_dd_setup_connection(self))
    {
      msg_error("Initiating connection failed, reconnecting",
                evt_tag_int("time_reopen", self->time_reopen));
      afsocket_dd_start_reconnect_timer(self);
      return;
    }
  self->reconnect_timer.handler = (void (*)(void *)) afsocket_dd_reconnect;
}

static gboolean
afsocket_dd_setup_proto_factory(AFSocketDestDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  if (!self->proto_factory)
    self->proto_factory = log_proto_client_get_factory(&cfg->plugin_context, self->transport_mapper->logproto);

  if (!self->proto_factory)
    {
      msg_error("Unknown value specified in the transport() option, no such LogProto plugin found",
                evt_tag_str("transport", self->transport_mapper->logproto));
      return FALSE;
    }
  return TRUE;
}

static gboolean
afsocket_dd_setup_writer_options(AFSocketDestDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  log_writer_options_init(&self->writer_options, cfg, 0);
  return TRUE;
}

static gboolean
afsocket_dd_setup_transport(AFSocketDestDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  if (!transport_mapper_apply_transport(self->transport_mapper, cfg))
    return FALSE;

  if (!afsocket_dd_setup_proto_factory(self))
    return FALSE;

  if (!afsocket_dd_setup_writer_options(self))
    return FALSE;
  return TRUE;
}

gboolean
afsocket_dd_setup_addresses_method(AFSocketDestDriver *self)
{
  return TRUE;
}

static void
_afsocket_dd_try_to_restore_writer(AFSocketDestDriver *self)
{
  /* If we are reinitializing an old config, an existing writer may be present */
  if (self->writer)
    return;

  ReloadStoreItem *item = cfg_persist_config_fetch(
                            log_pipe_get_config(&self->super.super.super),
                            afsocket_dd_format_connections_name(self));

  /* We don't have an item stored in the reload cache, which means */
  /* it is the first time when we try to initialize the writer */
  if (!item)
    return;

  if (_is_protocol_compatible_with_writer_after_reload(self, item))
    self->writer = _reload_store_item_release_writer(item);

  _reload_store_item_free(item);
}

LogWriter *
afsocket_dd_construct_writer_method(AFSocketDestDriver *self)
{
  guint32 writer_flags = 0;

  writer_flags |= LW_FORMAT_PROTO;
  if (self->transport_mapper->sock_type == SOCK_STREAM)
    writer_flags |= LW_DETECT_EOF;

  return log_writer_new(writer_flags, self->super.super.super.cfg);
}

static gboolean
afsocket_dd_setup_writer(AFSocketDestDriver *self)
{
  _afsocket_dd_try_to_restore_writer(self);

  if (!self->writer)
    {
      /* NOTE: we open our writer with no fd, so we can send messages down there
       * even while the connection is not established */

      self->writer = afsocket_dd_construct_writer(self);
    }
  log_pipe_set_config((LogPipe *)self->writer, log_pipe_get_config(&self->super.super.super));
  log_writer_set_options(self->writer, &self->super.super.super,
                         &self->writer_options,
                         self->super.super.id,
                         afsocket_dd_stats_instance(self));
  log_writer_set_queue(self->writer, log_dest_driver_acquire_queue(
                         &self->super, afsocket_dd_format_qfile_name(self)));

  if (!log_pipe_init((LogPipe *) self->writer))
    {
      log_pipe_unref((LogPipe *) self->writer);
      return FALSE;
    }

  log_pipe_append(&self->super.super.super, (LogPipe *) self->writer);
  return TRUE;
}

static gboolean
afsocket_dd_setup_connection(AFSocketDestDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  self->time_reopen = cfg->time_reopen;

  if (!log_writer_opened(self->writer))
    _dd_reconnect_with_current_addresses(self);

  self->connection_initialized = TRUE;
  return TRUE;
}

static gboolean
_finalize_init(gpointer arg)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *)arg;
  afsocket_dd_try_connect(self);
  return TRUE;
}

static gboolean
_dd_init_stream(AFSocketDestDriver *self)
{
  if (!afsocket_dd_setup_writer(self))
    return FALSE;

  return transport_mapper_async_init(self->transport_mapper, _finalize_init, self);
}

static gboolean
_dd_init_dgram(AFSocketDestDriver *self)
{
  if (!transport_mapper_init(self->transport_mapper))
    {
      return FALSE;
    }

  if (!afsocket_dd_setup_writer(self))
    {
      return FALSE;
    }

  return _finalize_init(self);
}

gboolean
afsocket_dd_init(LogPipe *s)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  if (!log_dest_driver_init_method(s) ||
      !afsocket_dd_setup_transport(self))
    {
      return FALSE;
    }

  if (self->transport_mapper->sock_type == SOCK_STREAM)
    {
      return _dd_init_stream(self);
    }

  return _dd_init_dgram(self);
}

static void
afsocket_dd_stop_writer(AFSocketDestDriver *self)
{
  if (self->writer)
    log_pipe_deinit((LogPipe *) self->writer);
}

static void
afsocket_dd_save_connection(AFSocketDestDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  if (self->connections_kept_alive_across_reloads)
    {
      ReloadStoreItem *item = _reload_store_item_new(self);
      cfg_persist_config_add(cfg, afsocket_dd_format_connections_name(self), item,
                             (GDestroyNotify)_reload_store_item_free, FALSE);
      self->writer = NULL;
    }
}

gboolean
afsocket_dd_deinit(LogPipe *s)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  afsocket_dd_stop_watches(self);
  afsocket_dd_stop_writer(self);

  if (self->connection_initialized)
    {
      afsocket_dd_save_connection(self);
    }

  return log_dest_driver_deinit_method(s);
}

static void
afsocket_dd_notify(LogPipe *s, gint notify_code, gpointer user_data)
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
                 evt_tag_int("time_reopen", self->time_reopen));
      afsocket_dd_start_reconnect_timer(self);
      break;
    default:
      break;
    }
}

void
afsocket_dd_free(LogPipe *s)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  log_writer_options_destroy(&self->writer_options);
  g_sockaddr_unref(self->bind_addr);
  g_sockaddr_unref(self->dest_addr);
  log_pipe_unref((LogPipe *) self->writer);
  transport_mapper_free(self->transport_mapper);
  socket_options_free(self->socket_options);
  log_dest_driver_free(s);
}

void
afsocket_dd_init_instance(AFSocketDestDriver *self,
                          SocketOptions *socket_options,
                          TransportMapper *transport_mapper,
                          GlobalConfig *cfg)
{
  log_dest_driver_init_instance(&self->super, cfg);

  log_writer_options_defaults(&self->writer_options);
  self->super.super.super.init = afsocket_dd_init;
  self->super.super.super.deinit = afsocket_dd_deinit;
  self->super.super.super.free_fn = afsocket_dd_free;
  self->super.super.super.notify = afsocket_dd_notify;
  self->super.super.super.generate_persist_name = afsocket_dd_format_name;
  self->setup_addresses = afsocket_dd_setup_addresses;
  self->construct_writer = afsocket_dd_construct_writer_method;
  self->transport_mapper = transport_mapper;
  self->socket_options = socket_options;
  self->connections_kept_alive_across_reloads = TRUE;
  self->time_reopen = cfg->time_reopen;
  self->connection_initialized = FALSE;


  self->writer_options.mark_mode = MM_GLOBAL;
  self->writer_options.stats_level = STATS_LEVEL0;
  self->writer_options.stats_source = self->transport_mapper->stats_source;
  afsocket_dd_init_watches(self);
}
