/*
 * Copyright (c) 2002-2018 Balabit
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

#include "afinet-dest-failover.h"
#include "messages.h"
#include "timeutils.h"
#include "afinet.h"

#include <iv.h>

struct _AFInetDestDriverFailover
{
  GList *servers;
  GList *current_server;
  GSockAddr *primary_addr;

  guint probe_interval;
  guint probes_required;
  guint probes_received;
  struct iv_timer timer;
  struct iv_fd fd;

  LogExprNode *owner_expression;
  FailoverTransportMapper failover_transport_mapper;
  gpointer on_primary_available_cookie;
  AFInetOnPrimaryAvailable on_primary_available_func;
};

static const int TCP_PROBE_INTERVAL_DEFAULT = 60;
static const int SUCCESSFUL_PROBES_REQUIRED_DEFAULT = 3;

void
afinet_dd_failover_set_tcp_probe_interval(AFInetDestDriverFailover *self, gint tcp_probe_interval)
{
  self->probe_interval = tcp_probe_interval;
}

void
afinet_dd_failover_set_successful_probes_required(AFInetDestDriverFailover *self, gint successful_probes_required)
{
  self->probes_required = successful_probes_required;
}

static GList *
_primary(AFInetDestDriverFailover *self)
{
  return g_list_first(self->servers);
}

static void
_afinet_dd_hand_over_connection_to_afsocket(AFInetDestDriverFailover *self)
{
  self->probes_received = 0;
  self->current_server = _primary(self);
  self->on_primary_available_func(self->on_primary_available_cookie, self->fd.fd, self->primary_addr);
  self->primary_addr = NULL;
  self->fd.fd = -1;
}

static void
_afinet_dd_start_failback_timer(AFInetDestDriverFailover *self)
{
  glong elapsed_time;

  iv_validate_now();
  elapsed_time = timespec_diff_msec(&iv_now, &(self->timer.expires));
  self->timer.expires = iv_now;

  if (elapsed_time < (self->probe_interval*1000))
    {
      timespec_add_msec(&self->timer.expires, (self->probe_interval*1000 - elapsed_time));
    }
  iv_timer_register(&self->timer);
}

static void
_afinet_dd_tcp_probe_succeded(AFInetDestDriverFailover *self)
{
  self->probes_received++;
  msg_notice("Probing primary server successful",
             evt_tag_int("successful-probes-received", self->probes_received),
             evt_tag_int("successful-probes-required", self->probes_required));

  if (self->probes_received >= self->probes_required)
    {
      msg_notice("Primary server seems to be stable, reconnecting to primary server");
      _afinet_dd_hand_over_connection_to_afsocket(self);
    }
  else
    {
      close(self->fd.fd);
      g_sockaddr_unref(self->primary_addr);
      _afinet_dd_start_failback_timer(self);
    }
}

static void
_afinet_dd_tcp_probe_failed(AFInetDestDriverFailover *self)
{
  self->probes_received = 0;
  g_sockaddr_unref(self->primary_addr);
  _afinet_dd_start_failback_timer(self);
}

static gboolean
_socket_succeeded(AFInetDestDriverFailover *self)
{
  int error = 0;
  socklen_t errorlen = sizeof(error);
  gchar buf[MAX_SOCKADDR_STRING];

  if (getsockopt(self->fd.fd, SOL_SOCKET, SO_ERROR, &error, &errorlen) == -1)
    {
      msg_error("getsockopt(SOL_SOCKET, SO_ERROR) failed for connecting socket",
                evt_tag_int("fd", self->fd.fd),
                evt_tag_str("server", g_sockaddr_format(self->primary_addr, buf, sizeof(buf), GSA_FULL)),
                evt_tag_error(EVT_TAG_OSERROR));
      return FALSE;
    }

  if (error)
    {
      msg_error("Connection towards primary server failed",
                evt_tag_int("fd", self->fd.fd),
                evt_tag_str("server", g_sockaddr_format(self->primary_addr, buf, sizeof(buf), GSA_FULL)),
                evt_tag_errno(EVT_TAG_OSERROR, error));
      close(self->fd.fd);
      return FALSE;
    }
  return TRUE;
}

static void
_afinet_dd_handle_tcp_probe_socket(gpointer s)
{
  AFInetDestDriverFailover *self = (AFInetDestDriverFailover *) s;

  if (iv_fd_registered(&self->fd))
    iv_fd_unregister(&self->fd);

  if (_socket_succeeded(self))
    _afinet_dd_tcp_probe_succeded(self);
  else
    _afinet_dd_tcp_probe_failed(self);
}

static gboolean
_connect_normal(GIOStatus iostatus)
{
  return G_IO_STATUS_NORMAL == iostatus;
}

static gboolean
_connect_in_progress(GIOStatus iostatus)
{
  return G_IO_STATUS_ERROR == iostatus && EINPROGRESS == errno;
}

static void
_afinet_dd_tcp_probe_primary_server(AFInetDestDriverFailover *self)
{
  GIOStatus iostatus = g_connect(self->fd.fd, self->primary_addr);
  if (_connect_normal(iostatus))
    {
      msg_notice("Successfully connected to primary");
      _afinet_dd_tcp_probe_succeded(self);
      return;
    }

  if (_connect_in_progress(iostatus))
    {
      iv_fd_register(&self->fd);
      return;
    }

  gchar buf[MAX_SOCKADDR_STRING];
  msg_error("Connection towards primary server failed",
            evt_tag_int("fd", self->fd.fd),
            evt_tag_str("server", g_sockaddr_format(self->primary_addr, buf, sizeof(buf), GSA_FULL)),
            evt_tag_error(EVT_TAG_OSERROR));
  close(self->fd.fd);
  _afinet_dd_tcp_probe_failed(self);
}

static const gchar *
_get_hostname(GList *l)
{
  return (const gchar *)(l->data);
}

static GSockAddr *
_resolve_hostname_with_transport_mapper(const gchar *hostname, TransportMapper *transport_mapper)
{
  GSockAddr *addr = NULL;
  if (!resolve_hostname_to_sockaddr(&addr, transport_mapper->address_family, hostname))
    {
      msg_warning("Unable to resolve the address of the primary server",
                  evt_tag_str("address", hostname));
      return NULL;
    }
  return addr;
}

static gint
_determine_port(AFInetDestDriverFailover *self)
{
  return afinet_determine_port(self->failover_transport_mapper.transport_mapper,
                               self->failover_transport_mapper.dest_port);
}

// TODO - use determine port result (in init)
// TODO - address-family
static gboolean
_resolve_primary_address(AFInetDestDriverFailover *self)
{
  self->primary_addr = _resolve_hostname_with_transport_mapper(_get_hostname(_primary(self)),
                       self->failover_transport_mapper.transport_mapper);
  if (!self->primary_addr)
    return FALSE;

  g_sockaddr_set_port(self->primary_addr, _determine_port(self));
  return TRUE;
}

static gboolean
_setup_failback_fd(AFInetDestDriverFailover *self)
{
  if (!transport_mapper_open_socket(self->failover_transport_mapper.transport_mapper,
                                    self->failover_transport_mapper.socket_options,
                                    self->failover_transport_mapper.bind_addr,
                                    AFSOCKET_DIR_SEND, &self->fd.fd))
    {
      msg_error("Error creating socket for tcp-probe the primary server",
                evt_tag_error(EVT_TAG_OSERROR));
      return FALSE;
    }
  return TRUE;
}

static void
_afinet_dd_failback_timer_elapsed(void *cookie)
{
  AFInetDestDriverFailover *self = (AFInetDestDriverFailover *) cookie;

  msg_notice("Probing the primary server.",
             evt_tag_int("tcp-probe-interval", self->probe_interval));

  iv_validate_now();
  self->timer.expires = iv_now; // register starting time, required for "elapsed_time"

  if (!_resolve_primary_address(self))
    {
      _afinet_dd_tcp_probe_failed(self);
      return;
    }

  if (!_setup_failback_fd(self))
    {
      _afinet_dd_tcp_probe_failed(self);
      return;
    }

  _afinet_dd_tcp_probe_primary_server(self);
}

static gboolean
_is_failback_enabled(AFInetDestDriverFailover *self)
{
  return self->on_primary_available_func != NULL;
}

static void
_init_current_server(AFInetDestDriverFailover *self )
{
  self->current_server = _is_failback_enabled(self) ? g_list_next(_primary(self)) : _primary(self);

  if (_primary(self) == self->current_server)
    {
      msg_warning("Last failover server reached, trying the original host again",
                  evt_tag_str("host", _get_hostname(self->current_server)),
                  log_expr_node_location_tag(self->owner_expression));
    }
  else
    {
      msg_warning("Last failover server reached, trying the first failover again",
                  evt_tag_str("next_failover_server", _get_hostname(self->current_server)),
                  log_expr_node_location_tag(self->owner_expression));
    }
}

static void
_step_current_server_iterator(AFInetDestDriverFailover *self)
{
  GList *previous = self->current_server;
  self->current_server = g_list_next(self->current_server);

  if (!self->current_server)
    {
      _init_current_server(self);
      return;
    }

  if (_is_failback_enabled(self) && _primary(self) == previous)
    {
      _afinet_dd_start_failback_timer(self);
      msg_warning("Current primary server is inaccessible, sending the messages to the next failover server",
                  evt_tag_str("next_failover_server", _get_hostname(self->current_server)),
                  log_expr_node_location_tag(self->owner_expression));
      return;
    }

  msg_warning("Current failover server is inaccessible, sending the messages to the next failover server",
              evt_tag_str("next_failover_server", _get_hostname(self->current_server)),
              log_expr_node_location_tag(self->owner_expression));
}


const gchar *
afinet_dd_failover_get_hostname(AFInetDestDriverFailover *self)
{
  if (self->current_server == NULL)
    {
      return _get_hostname(_primary(self));
    }

  return _get_hostname(self->current_server);
}

void
afinet_dd_failover_next(AFInetDestDriverFailover *self)
{
  if (!self->current_server)
    {
      self->current_server = _primary(self);
      return;
    }

  _step_current_server_iterator(self);
}

void
afinet_dd_failover_add_servers(AFInetDestDriverFailover *self, GList *failovers)
{
  self->servers = g_list_concat(self->servers, failovers);
}

void
afinet_dd_failover_enable_failback(AFInetDestDriverFailover *self, gpointer cookie,
                                   AFInetOnPrimaryAvailable callback_function)
{
  self->on_primary_available_cookie = cookie;
  self->on_primary_available_func = callback_function;
}

static void
_afinet_dd_init_failback_handlers(AFInetDestDriverFailover *self)
{
  IV_TIMER_INIT(&self->timer);
  self->timer.cookie = self;
  self->timer.handler = _afinet_dd_failback_timer_elapsed;

  IV_FD_INIT(&self->fd);
  self->fd.cookie = self;
  self->fd.handler_out = (void (*)(void *)) _afinet_dd_handle_tcp_probe_socket;
}

static void
_afinet_dd_stop_failback_handlers(AFInetDestDriverFailover *self)
{
  if (iv_timer_registered(&self->timer))
    iv_timer_unregister(&self->timer);

  if (iv_fd_registered(&self->fd))
    {
      iv_fd_unregister(&self->fd);
      close(self->fd.fd);
    }
}

void
afinet_dd_failover_init(AFInetDestDriverFailover *self, const gchar *primary,
                        LogExprNode *owner_expr, FailoverTransportMapper *failover_transport_mapper)
{
  self->servers = g_list_prepend(self->servers, g_strdup(primary));
  self->owner_expression = owner_expr;
  self->failover_transport_mapper = *failover_transport_mapper;
  _afinet_dd_init_failback_handlers(self);
}

AFInetDestDriverFailover *
afinet_dd_failover_new(void)
{
  AFInetDestDriverFailover *self = g_new0(AFInetDestDriverFailover, 1);
  self->probe_interval = TCP_PROBE_INTERVAL_DEFAULT;
  self->probes_required = SUCCESSFUL_PROBES_REQUIRED_DEFAULT;
  return self;
}

void
afinet_dd_failover_free(AFInetDestDriverFailover *self)
{
  if (!self)
    return;

  _afinet_dd_stop_failback_handlers(self);
  g_list_free_full(self->servers, g_free);
  g_sockaddr_unref(self->primary_addr);
}
