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

#ifndef AFINET_DEST_FAILOVER_H_INCLUDED
#define AFINET_DEST_FAILOVER_H_INCLUDED

#include "syslog-ng.h"
#include "gsocket.h"
#include "transport-mapper-inet.h"
#include "cfg.h"

typedef struct _AFInetDestDriverFailover AFInetDestDriverFailover;
typedef void (*AFInetOnPrimaryAvailable)(gpointer cookie, gint fd, GSockAddr *addr);
typedef struct _FailoverTransportMapper
{
  TransportMapper *transport_mapper;
  SocketOptions *socket_options;
  const gchar *dest_port;
  const gchar *bind_ip;
  const gchar *bind_port;
} FailoverTransportMapper;

AFInetDestDriverFailover *
afinet_dd_failover_new(const gchar *primary);

void
afinet_dd_failover_init(AFInetDestDriverFailover *self, LogExprNode *owner_expr,
                        FailoverTransportMapper *failover_transport_mapper);

void afinet_dd_failover_free(AFInetDestDriverFailover *self);

void afinet_dd_failover_enable_failback(AFInetDestDriverFailover *self, gpointer cookie,
                                        AFInetOnPrimaryAvailable callback);
void afinet_dd_failover_add_servers(AFInetDestDriverFailover *self, GList *failovers);
void afinet_dd_failover_set_tcp_probe_interval(AFInetDestDriverFailover *self, gint tcp_probe_interval);
void afinet_dd_failover_set_successful_probes_required(AFInetDestDriverFailover *self, gint successful_probes_required);

void afinet_dd_failover_next(AFInetDestDriverFailover *self);
const gchar *afinet_dd_failover_get_hostname(AFInetDestDriverFailover *self);


#endif
