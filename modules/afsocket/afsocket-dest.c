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
#include "timeutils/misc.h"
#include "hostname.h"
#include "persist-state.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster-single.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef struct _ReloadStoreItem
{
  LogProtoClientFactory *proto_factory;
  GSockAddr *dest_addr;
  LogWriter *writer;
} ReloadStoreItem;

static ReloadStoreItem *
_reload_store_item_new(AFSocketDestDriver *afsocket_dd)
{
  ReloadStoreItem *item = g_new(ReloadStoreItem, 1);
  item->proto_factory = afsocket_dd->proto_factory;
  item->writer = afsocket_dd->writer;
  item->dest_addr = g_sockaddr_ref(afsocket_dd->dest_addr);
  return item;
}

static void
_reload_store_item_free(ReloadStoreItem *self)
{
  if (!self)
    return;

  if (self->writer)
    log_pipe_unref((LogPipe *) self->writer);

  g_sockaddr_unref(self->dest_addr);
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

void
afsocket_dd_set_close_on_input(LogDriver *s, gboolean close_on_input)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  self->close_on_input = close_on_input;
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
_get_legacy_module_identifier(const AFSocketDestDriver *self)
{
  static gchar legacy_module_identifier[128];
  const gchar *hostname = get_local_hostname_fqdn();

  g_snprintf(legacy_module_identifier, sizeof(legacy_module_identifier), "%s,%s,%s",
             (self->transport_mapper->sock_type == SOCK_STREAM) ? "stream" : "dgram",
             afsocket_dd_get_dest_name(self),
             hostname);

  return legacy_module_identifier;
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

static const gchar *
afsocket_dd_format_legacy_connection_name(const AFSocketDestDriver *self)
{
  static gchar legacy_persist_name[1024];
  g_snprintf(legacy_persist_name, sizeof(legacy_persist_name), "%s_connection(%s)", _module_name,
             _get_legacy_module_identifier(self));

  return legacy_persist_name;
}

static gchar *
afsocket_dd_stats_instance(AFSocketDestDriver *self)
{
  static gchar buf[256];

  g_snprintf(buf, sizeof(buf), "%s,%s", self->transport_mapper->transport, afsocket_dd_get_dest_name(self));
  return buf;
}

static void _afsocket_dd_connection_in_progress(AFSocketDestDriver *self);

static void
afsocket_dd_init_watches(AFSocketDestDriver *self)
{
  IV_FD_INIT(&self->connect_fd);
  self->connect_fd.cookie = self;
  self->connect_fd.handler_out = (void (*)(void *)) _afsocket_dd_connection_in_progress;

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

void
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
      self->fd = -1;
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
  timespec_add_msec(&self->reconnect_timer.expires, self->writer_options.time_reopen * 1000L);
  iv_timer_register(&self->reconnect_timer);

  stats_counter_set(self->metrics.output_unreachable, 1);
}

static gboolean
_update_legacy_connection_persist_name(AFSocketDestDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);
  const gchar *current_persist_name = afsocket_dd_format_connections_name(self);
  const gchar *legacy_persist_name = afsocket_dd_format_legacy_connection_name(self);

  if (persist_state_entry_exists(cfg->state, current_persist_name))
    return TRUE;

  if (!persist_state_entry_exists(cfg->state, legacy_persist_name))
    return TRUE;

  return persist_state_move_entry(cfg->state, legacy_persist_name, current_persist_name);
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
  LogTransport *transport;
  LogProtoClient *proto;
  gchar buf1[256], buf2[256];

  main_loop_assert_main_thread();

  stats_counter_set(self->metrics.output_unreachable, 0);
  msg_notice("Syslog connection established",
             evt_tag_int("fd", self->fd),
             evt_tag_str("server", g_sockaddr_format(self->dest_addr, buf2, sizeof(buf2), GSA_FULL)),
             evt_tag_str("local", g_sockaddr_format(self->bind_addr, buf1, sizeof(buf1), GSA_FULL)));

  transport = afsocket_dd_construct_transport(self, self->fd);
  if (!transport)
    return FALSE;

  proto = log_proto_client_factory_construct(self->proto_factory, transport, &self->writer_options.proto_options.super);

  log_proto_client_restart_with_state(proto, cfg->state, afsocket_dd_format_connections_name(self));
  log_writer_reopen(self->writer, proto);
  return TRUE;
}

void
afsocket_dd_connected_with_fd(gpointer s, gint fd, GSockAddr *saddr)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *)s;
  afsocket_dd_stop_watches(self);
  g_sockaddr_unref(self->dest_addr);
  self->dest_addr = saddr;
  self->fd = fd;
  if (!afsocket_dd_connected(self))
    {
      close(self->fd);
      self->fd = -1;
      afsocket_dd_start_reconnect_timer(self);
    }
}

static void
_afsocket_dd_connection_in_progress(AFSocketDestDriver *self)
{
  gchar buf[256];
  int error = 0;
  socklen_t errorlen = sizeof(error);

  if (iv_fd_registered(&self->connect_fd))
    iv_fd_unregister(&self->connect_fd);

  if (self->transport_mapper->sock_type == SOCK_STREAM)
    {
      if (getsockopt(self->fd, SOL_SOCKET, SO_ERROR, &error, &errorlen) == -1)
        {
          msg_error("getsockopt(SOL_SOCKET, SO_ERROR) failed for connecting socket",
                    evt_tag_int("fd", self->fd),
                    evt_tag_str("server", g_sockaddr_format(self->dest_addr, buf, sizeof(buf), GSA_FULL)),
                    evt_tag_error(EVT_TAG_OSERROR),
                    evt_tag_int("time_reopen", self->writer_options.time_reopen));
          goto error_reconnect;
        }
      if (error)
        {
          msg_error("Syslog connection failed",
                    evt_tag_int("fd", self->fd),
                    evt_tag_str("server", g_sockaddr_format(self->dest_addr, buf, sizeof(buf), GSA_FULL)),
                    evt_tag_errno(EVT_TAG_OSERROR, error),
                    evt_tag_int("time_reopen", self->writer_options.time_reopen));
          goto error_reconnect;
        }
    }

  if (afsocket_dd_connected(self))
    return;

error_reconnect:
  close(self->fd);
  self->fd = -1;
  afsocket_dd_start_reconnect_timer(self);
}

static gboolean
afsocket_dd_start_connect(AFSocketDestDriver *self)
{
  int sock, rc;
  gchar buf1[MAX_SOCKADDR_STRING], buf2[MAX_SOCKADDR_STRING];

  main_loop_assert_main_thread();

  if (log_writer_opened(self->writer))
    return TRUE;

  g_assert(self->transport_mapper->transport);
  g_assert(self->bind_addr);
  g_assert(self->dest_addr);

  if (!transport_mapper_open_socket(self->transport_mapper, self->socket_options, self->bind_addr, self->dest_addr,
                                    AFSOCKET_DIR_SEND, &sock))
    {
      return FALSE;
    }

  if (!socket_options_setup_peer_socket(self->socket_options, sock, self->dest_addr))
    return FALSE;

  rc = g_connect(sock, self->dest_addr);
  if (rc == G_IO_STATUS_NORMAL)
    {
      self->fd = sock;
      if (!afsocket_dd_connected(self))
        {
          close(self->fd);
          self->fd = -1;
          return FALSE;
        }
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

void
afsocket_dd_reconnect(AFSocketDestDriver *self)
{
  if (!afsocket_dd_setup_addresses(self) || !afsocket_dd_start_connect(self))
    {
      msg_error("Initiating connection failed, reconnecting",
                evt_tag_int("time_reopen", self->writer_options.time_reopen));
      afsocket_dd_start_reconnect_timer(self);
    }
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

  self->transport_mapper->create_multitransport = self->proto_factory->use_multitransport;

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

static gboolean
_afsocket_dd_try_to_restore_connection_state(AFSocketDestDriver *self)
{
  /* If we are reinitializing an old config, an existing writer may be present */
  if (self->writer)
    return TRUE;

  ReloadStoreItem *item = cfg_persist_config_fetch(
                            log_pipe_get_config(&self->super.super.super),
                            afsocket_dd_format_connections_name(self));

  /* We don't have an item stored in the reload cache, which means */
  /* it is the first time when we try to initialize the writer */
  if (!item)
    return FALSE;

  if (_is_protocol_compatible_with_writer_after_reload(self, item))
    self->writer = _reload_store_item_release_writer(item);

  self->dest_addr = g_sockaddr_ref(item->dest_addr);
  _reload_store_item_free(item);
  return TRUE;
}

LogWriter *
afsocket_dd_construct_writer_method(AFSocketDestDriver *self)
{
  guint32 writer_flags = 0;

  writer_flags |= LW_FORMAT_PROTO;
  if (self->transport_mapper->sock_type == SOCK_STREAM && self->close_on_input)
    writer_flags |= LW_DETECT_EOF;

  LogWriter *writer = log_writer_new(writer_flags, self->super.super.super.cfg);
  log_pipe_set_options((LogPipe *) writer, &self->super.super.super.options);

  return writer;
}

static void
_init_stats_key_builders(AFSocketDestDriver *self, StatsClusterKeyBuilder **writer_sck_builder,
                         StatsClusterKeyBuilder **driver_sck_builder, StatsClusterKeyBuilder **queue_sck_builder)
{
  *writer_sck_builder = stats_cluster_key_builder_new();
  stats_cluster_key_builder_add_label(*writer_sck_builder, stats_cluster_label("driver", "afsocket"));
  stats_cluster_key_builder_add_legacy_label(*writer_sck_builder, stats_cluster_label("transport",
                                             self->transport_mapper->transport));
  stats_cluster_key_builder_add_legacy_label(*writer_sck_builder, stats_cluster_label("address",
                                             afsocket_dd_get_dest_name(self)));

  *driver_sck_builder = stats_cluster_key_builder_new();
  stats_cluster_key_builder_add_label(*driver_sck_builder, stats_cluster_label("driver", "afsocket"));
  stats_cluster_key_builder_add_label(*driver_sck_builder, stats_cluster_label("id", self->super.super.id));
  stats_cluster_key_builder_add_legacy_label(*driver_sck_builder, stats_cluster_label("transport",
                                             self->transport_mapper->transport));
  stats_cluster_key_builder_add_legacy_label(*driver_sck_builder, stats_cluster_label("address",
                                             afsocket_dd_get_dest_name(self)));
  stats_cluster_key_builder_set_legacy_alias(*driver_sck_builder,
                                             self->writer_options.stats_source | SCS_DESTINATION,
                                             self->super.super.id, afsocket_dd_stats_instance(self));

  *queue_sck_builder = stats_cluster_key_builder_new();
  stats_cluster_key_builder_add_label(*queue_sck_builder, stats_cluster_label("driver", "afsocket"));
  stats_cluster_key_builder_add_label(*queue_sck_builder, stats_cluster_label("id", self->super.super.id));
  stats_cluster_key_builder_add_legacy_label(*queue_sck_builder, stats_cluster_label("transport",
                                             self->transport_mapper->transport));
  stats_cluster_key_builder_add_legacy_label(*queue_sck_builder, stats_cluster_label("address",
                                             afsocket_dd_get_dest_name(self)));
}

static void
afsocket_dd_register_stats(AFSocketDestDriver *self)
{
  StatsClusterLabel labels[] =
  {
    stats_cluster_label("id", self->super.super.id),
    stats_cluster_label("driver", "afsocket"),
    stats_cluster_label("transport", self->transport_mapper->transport),
    stats_cluster_label("address", afsocket_dd_get_dest_name(self)),
  };

  gint level = log_pipe_is_internal(&self->super.super.super) ? STATS_LEVEL3 : STATS_LEVEL0;
  StatsClusterKey sc_key;
  stats_cluster_single_key_set(&sc_key, "output_unreachable", labels, G_N_ELEMENTS(labels));

  stats_lock();
  stats_register_counter(level, &sc_key, SC_TYPE_SINGLE_VALUE, &self->metrics.output_unreachable);
  stats_unlock();
}

static void
afsocket_dd_unregister_stats(AFSocketDestDriver *self)
{
  StatsClusterLabel labels[] =
  {
    stats_cluster_label("id", self->super.super.id),
    stats_cluster_label("driver", "afsocket"),
    stats_cluster_label("transport", self->transport_mapper->transport),
    stats_cluster_label("address", afsocket_dd_get_dest_name(self)),
  };

  StatsClusterKey sc_key;
  stats_cluster_single_key_set(&sc_key, "output_unreachable", labels, G_N_ELEMENTS(labels));

  stats_lock();
  stats_unregister_counter(&sc_key, SC_TYPE_SINGLE_VALUE, &self->metrics.output_unreachable);
  stats_unlock();
}

static gboolean
afsocket_dd_setup_writer(AFSocketDestDriver *self)
{
  gboolean kept_alive_connection = _afsocket_dd_try_to_restore_connection_state(self);

  if (!self->writer)
    {
      /* NOTE: we open our writer with no fd, so we can send messages down there
       * even while the connection is not established */

      self->writer = afsocket_dd_construct_writer(self);
    }

  StatsClusterKeyBuilder *writer_sck_builder;
  StatsClusterKeyBuilder *driver_sck_builder;
  StatsClusterKeyBuilder *queue_sck_builder;
  _init_stats_key_builders(self, &writer_sck_builder, &driver_sck_builder, &queue_sck_builder);

  log_pipe_set_config((LogPipe *)self->writer, log_pipe_get_config(&self->super.super.super));
  log_writer_set_options(self->writer, &self->super.super.super,
                         &self->writer_options,
                         self->super.super.id,
                         writer_sck_builder);

  gint stats_level = log_pipe_is_internal(&self->super.super.super) ? STATS_LEVEL3 : self->writer_options.stats_level;
  LogQueue *queue = log_dest_driver_acquire_queue(&self->super, afsocket_dd_format_qfile_name(self),
                                                  stats_level, driver_sck_builder, queue_sck_builder);
  log_writer_set_queue(self->writer, queue);

  stats_cluster_key_builder_free(queue_sck_builder);
  stats_cluster_key_builder_free(driver_sck_builder);

  if (!log_pipe_init((LogPipe *) self->writer))
    {
      log_pipe_unref((LogPipe *) self->writer);
      return FALSE;
    }
  log_pipe_append(&self->super.super.super, (LogPipe *) self->writer);

  if (kept_alive_connection)
    {
      LogProtoClient *proto = log_writer_steal_proto(self->writer);

      if (proto)
        {
          self->fd = log_proto_client_get_fd(proto);
          log_writer_reopen(self->writer, proto);
        }
    }
  self->connection_initialized = TRUE;
  return TRUE;
}

static gboolean
_finalize_init(gpointer arg)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *)arg;
  afsocket_dd_reconnect(self);
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

static void
_dd_rewind_stateless_proto_backlog(AFSocketDestDriver *self)
{
  if (!log_proto_client_factory_is_proto_stateful(self->proto_factory))
    {
      log_writer_msg_rewind(self->writer);
    }
}

static gboolean
_dd_init_socket(AFSocketDestDriver *self)
{
  switch (self->transport_mapper->sock_type)
    {
    case SOCK_STREAM:
      return _dd_init_stream(self);

    case SOCK_DGRAM:
    default:
      return _dd_init_dgram(self);
    }
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

  if (!_update_legacy_connection_persist_name(self))
    return FALSE;

  afsocket_dd_register_stats(self);

  if (!_dd_init_socket(self))
    {
      return FALSE;
    }

  _dd_rewind_stateless_proto_backlog(self);

  return TRUE;
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
                             (GDestroyNotify)_reload_store_item_free);
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

  afsocket_dd_unregister_stats(self);
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

      msg_notice((notify_code == NC_CLOSE) ? "Syslog connection closed" : "Syslog connection broken",
                 evt_tag_int("fd", self->fd),
                 evt_tag_str("server", g_sockaddr_format(self->dest_addr, buf, sizeof(buf), GSA_FULL)),
                 evt_tag_int("time_reopen", self->writer_options.time_reopen));
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
  self->setup_addresses = afsocket_dd_setup_addresses_method;
  self->construct_writer = afsocket_dd_construct_writer_method;
  self->transport_mapper = transport_mapper;
  self->socket_options = socket_options;
  self->connections_kept_alive_across_reloads = TRUE;
  self->close_on_input = TRUE;
  self->connection_initialized = FALSE;


  self->writer_options.mark_mode = MM_GLOBAL;
  self->writer_options.stats_level = STATS_LEVEL0;
  self->writer_options.stats_source = self->transport_mapper->stats_source;
  afsocket_dd_init_watches(self);
}
