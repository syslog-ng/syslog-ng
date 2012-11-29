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

#ifndef AFINET_DEST_H_INCLUDED
#define AFINET_DEST_H_INCLUDED

#include "afinet.h"
#include "afsocket-dest.h"

#if ENABLE_SPOOF_SOURCE

/* this forward declaration avoids having to include libnet, which requires
 * ugly playing with macros, see that for yourself in the implementation
 * file */
typedef struct libnet_context libnet_t;
#endif

typedef struct _AFInetDestDriver
{
  AFSocketDestDriver super;
  InetSocketOptions sock_options;
#if ENABLE_SPOOF_SOURCE
  gboolean spoof_source;
  libnet_t *lnet_ctx;
  GStaticMutex lnet_lock;
  GString *lnet_buffer;
#endif
  /* character as it can contain a service name from /etc/services */
  gchar *bind_port;
  gchar *bind_ip;
  /* character as it can contain a service name from /etc/services */
  gchar *dest_port;
  /* destination hostname is stored in super.hostname */
} AFInetDestDriver;

void afinet_dd_set_localport(LogDriver *self, gchar *service);
void afinet_dd_set_destport(LogDriver *self, gchar *service);
void afinet_dd_set_localip(LogDriver *self, gchar *ip);
void afinet_dd_set_sync_freq(LogDriver *self, gint sync_freq);
void afinet_dd_set_spoof_source(LogDriver *self, gboolean enable);

LogDriver *afinet_dd_new(gint af, gint sock_type, gchar *host);
LogDriver *afsyslog_dd_new(gchar *host);

#endif
