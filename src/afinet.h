/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef AFINET_H_INCLUDED
#define AFINET_H_INCLUDED

#include "afsocket.h"

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
void afinet_sd_set_localport(LogDriver *self, gint port, gchar *service, gchar *proto);
void afinet_sd_set_localip(LogDriver *self, gchar *ip);

#define afinet_sd_set_auth(a,b)
#define afinet_sd_set_mac(a,b)
#define afinet_sd_set_encrypt(a,b)

typedef struct _AFInetDestDriver
{
  AFSocketDestDriver super;
  InetSocketOptions sock_options;
} AFInetDestDriver;

LogDriver *afinet_dd_new(gint af, gchar *host, gint port, guint flags);
void afinet_dd_set_localport(LogDriver *self, gint port, gchar *service, gchar *proto);
void afinet_dd_set_destport(LogDriver *self, gint port, gchar *service, gchar *proto);
void afinet_dd_set_localip(LogDriver *self, gchar *ip);
void afinet_dd_set_sync_freq(LogDriver *self, gint sync_freq);

#endif
