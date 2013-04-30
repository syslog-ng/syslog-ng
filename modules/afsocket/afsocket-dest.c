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
#include "gsocket.h"
#include "stats.h"
#include "mainloop.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

void
afsocket_dd_set_keep_alive(LogDriver *s, gboolean enable)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  self->connections_kept_alive_accross_reloads = enable;
}


static gchar *
afsocket_dd_format_persist_name(AFSocketDestDriver *self, gboolean qfile)
{
  static gchar persist_name[256];

  g_snprintf(persist_name, sizeof(persist_name),
             qfile ? "afsocket_dd_qfile(%s,%s)" : "afsocket_dd_connection(%s,%s)",
             (self->transport_mapper->sock_type == SOCK_STREAM) ? "stream" : "dgram",
             afsocket_dd_get_dest_name(self));
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

  if (self->transport_mapper->sock_type == SOCK_STREAM)
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

  transport = afsocket_dd_construct_transport(self, self->fd);
  if (!transport)
    goto error_reconnect;

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

  g_assert(self->transport_mapper->transport);
  g_assert(self->bind_addr);

  if (!transport_mapper_open_socket(self->transport_mapper, self->socket_options, self->bind_addr, AFSOCKET_DIR_SEND, &sock))
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
  if (!afsocket_dd_setup_addresses(self) ||
      !afsocket_dd_start_connect(self))
    {
      msg_error("Initiating connection failed, reconnecting",
                evt_tag_int("time_reopen", self->time_reopen),
                NULL);
      afsocket_dd_start_reconnect_timer(self);
    }
}

static gboolean
afsocket_dd_setup_proto_factory(AFSocketDestDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  self->proto_factory = log_proto_client_get_factory(cfg, self->transport_mapper->logproto);
  if (!self->proto_factory)
    {
      msg_error("Unknown value specified in the transport() option, no such LogProto plugin found",
                evt_tag_str("transport", self->transport_mapper->logproto),
                NULL);
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
afsocket_dd_restore_connection(AFSocketDestDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  self->writer = cfg_persist_config_fetch(cfg, afsocket_dd_format_persist_name(self, FALSE));
}

LogTransport *
afsocket_dd_construct_transport_method(AFSocketDestDriver *self, gint fd)
{
  if (self->transport_mapper->sock_type == SOCK_DGRAM)
    return log_transport_dgram_socket_new(fd);
  else
    return log_transport_stream_socket_new(fd);
}

LogWriter *
afsocket_dd_construct_writer_method(AFSocketDestDriver *self)
{
  guint32 writer_flags = 0;

  writer_flags |= LW_FORMAT_PROTO;
  if ((self->transport_mapper->sock_type == SOCK_STREAM))
    writer_flags |= LW_DETECT_EOF;

  return log_writer_new(writer_flags);
}

static void
afsocket_dd_setup_writer(AFSocketDestDriver *self)
{
  if (!self->writer)
    {
      /* NOTE: we open our writer with no fd, so we can send messages down there
       * even while the connection is not established */

      self->writer = afsocket_dd_construct_writer(self);
    }
  log_writer_set_options(self->writer, &self->super.super.super,
                         &self->writer_options,
                         STATS_LEVEL0,
                         self->transport_mapper->stats_source,
                         self->super.super.id,
                         afsocket_dd_stats_instance(self));
  log_writer_set_queue(self->writer, log_dest_driver_acquire_queue(&self->super, afsocket_dd_format_persist_name(self, TRUE)));

  log_pipe_init((LogPipe *) self->writer, NULL);
  log_pipe_append(&self->super.super.super, (LogPipe *) self->writer);
}

static gboolean
afsocket_dd_setup_connection(AFSocketDestDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  self->time_reopen = cfg->time_reopen;

  afsocket_dd_restore_connection(self);
  afsocket_dd_setup_writer(self);

  if (!log_writer_opened(self->writer))
    afsocket_dd_reconnect(self);
  return TRUE;
}

gboolean
afsocket_dd_init(LogPipe *s)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  return log_dest_driver_init_method(s) &&
         afsocket_dd_setup_transport(self) &&
         afsocket_dd_setup_addresses(self) &&
         afsocket_dd_setup_connection(self);
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

  if (self->connections_kept_alive_accross_reloads)
    {
      cfg_persist_config_add(cfg, afsocket_dd_format_persist_name(self, FALSE), self->writer, (GDestroyNotify) log_pipe_unref, FALSE);
      self->writer = NULL;
    }
}

gboolean
afsocket_dd_deinit(LogPipe *s)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  afsocket_dd_stop_watches(self);
  afsocket_dd_stop_writer(self);
  afsocket_dd_save_connection(self);

  return log_dest_driver_deinit_method(s);
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

void
afsocket_dd_free(LogPipe *s)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  log_writer_options_destroy(&self->writer_options);
  g_sockaddr_unref(self->bind_addr);
  g_sockaddr_unref(self->dest_addr);
  log_pipe_unref((LogPipe *) self->writer);
  transport_mapper_free(self->transport_mapper);
  log_dest_driver_free(s);
}

void
afsocket_dd_init_instance(AFSocketDestDriver *self,
                          SocketOptions *socket_options,
                          TransportMapper *transport_mapper)
{
  log_dest_driver_init_instance(&self->super);

  log_writer_options_defaults(&self->writer_options);
  self->super.super.super.init = afsocket_dd_init;
  self->super.super.super.deinit = afsocket_dd_deinit;
  /* NULL behaves as if log_msg_forward_msg was specified */
  self->super.super.super.queue = NULL;
  self->super.super.super.free_fn = afsocket_dd_free;
  self->super.super.super.notify = afsocket_dd_notify;
  self->setup_addresses = afsocket_dd_setup_addresses;
  self->construct_transport = afsocket_dd_construct_transport_method;
  self->construct_writer = afsocket_dd_construct_writer_method;
  self->transport_mapper = transport_mapper;
  self->socket_options = socket_options;
  self->connections_kept_alive_accross_reloads = TRUE;


  self->writer_options.mark_mode = MM_GLOBAL;
  afsocket_dd_init_watches(self);
}
