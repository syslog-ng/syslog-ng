/*
 * Copyright (c) 2002-2012 Balabit
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

#ifndef AFINET_SOURCE_H_INCLUDED
#define AFINET_SOURCE_H_INCLUDED

#include "afinet.h"
#include "afsocket-source.h"
#include "tlscontext.h"

typedef struct _AFInetSourceDriver
{
  AFSocketSourceDriver super;
  /* character as it can contain a service name from /etc/services */
  gchar *bind_port;
  gchar *bind_ip;
} AFInetSourceDriver;

void afinet_sd_set_tls_context(LogDriver *s, TLSContext *tls_context);

AFInetSourceDriver *afinet_sd_new_tcp(GlobalConfig *cfg);
AFInetSourceDriver *afinet_sd_new_tcp6(GlobalConfig *cfg);
AFInetSourceDriver *afinet_sd_new_udp(GlobalConfig *cfg);
AFInetSourceDriver *afinet_sd_new_udp6(GlobalConfig *cfg);
AFInetSourceDriver *afinet_sd_new_syslog(GlobalConfig *cfg);
AFInetSourceDriver *afinet_sd_new_network(GlobalConfig *cfg);

void afinet_sd_set_localport(LogDriver *self, gchar *service);
void afinet_sd_set_localip(LogDriver *self, gchar *ip);

#endif
