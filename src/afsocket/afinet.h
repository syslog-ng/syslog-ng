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

#ifndef AFINET_H_INCLUDED
#define AFINET_H_INCLUDED

#include "afsocket.h"

#ifdef _GNU_SOURCE
#  define _GNU_SOURCE_DEFINED 1
#  undef _GNU_SOURCE
#endif

#if ENABLE_SPOOF_SOURCE
#include <libnet.h>
#endif

#if _GNU_SOURCE_DEFINED
#  undef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif


typedef struct _InetSocketOptions
{
  SocketOptions super;
  gint ttl;
  gint tos;
} InetSocketOptions;

typedef struct _AFInetSourceDriver
{
  AFSocketSourceDriver super;
  InetSocketOptions sock_options;
} AFInetSourceDriver;

LogDriver *afinet_sd_new(gint af, gchar *host, gint port, guint flags);
void afinet_sd_set_localport(LogDriver *self, gchar *service, const gchar *proto);
void afinet_sd_set_localip(LogDriver *self, gchar *ip);
void afinet_sd_set_transport(LogDriver *s, const gchar *transport);

typedef struct _AFInetDestDriver
{
  AFSocketDestDriver super;
  InetSocketOptions sock_options;
#if ENABLE_SPOOF_SOURCE
  gboolean spoof_source;
  libnet_t *lnet_ctx;
#endif
} AFInetDestDriver;

LogDriver *afinet_dd_new(gint af, gchar *host, gint port, guint flags);
void afinet_dd_set_localport(LogDriver *self, gchar *service, const gchar *proto);
void afinet_dd_set_destport(LogDriver *self, gchar *service, const gchar *proto);
void afinet_dd_set_localip(LogDriver *self, gchar *ip);
void afinet_dd_set_sync_freq(LogDriver *self, gint sync_freq);
void afinet_dd_set_spoof_source(LogDriver *self, gboolean enable);
void afinet_dd_set_transport(LogDriver *s, const gchar *transport);

static inline const gchar *
afinet_sd_get_proto_name(LogDriver *s)
{
  AFInetSourceDriver *self = (AFInetSourceDriver *) s;

  if (self->super.flags & AFSOCKET_DGRAM)
    return "udp";
  else
    return "tcp";
}

static inline const gchar *
afinet_dd_get_proto_name(LogDriver *s)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;

  if (self->super.flags & AFSOCKET_DGRAM)
    return "udp";
  else
    return "tcp";
}

#endif
