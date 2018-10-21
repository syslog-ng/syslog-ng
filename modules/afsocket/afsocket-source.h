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

#ifndef AFSOCKET_SOURCE_H_INCLUDED
#define AFSOCKET_SOURCE_H_INCLUDED

#include "afsocket.h"
#include "socket-options.h"
#include "transport-mapper.h"
#include "driver.h"
#include "logreader.h"

#include <iv.h>

typedef struct _AFSocketSourceDriver AFSocketSourceDriver;

struct _AFSocketSourceDriver
{
  LogSrcDriver super;
  guint32 connections_kept_alive_across_reloads:1,
          window_size_initialized:1;
  struct iv_fd listen_fd;
  gint fd;
  LogReaderOptions reader_options;
  LogProtoServerFactory *proto_factory;
  GSockAddr *bind_addr;
  gint max_connections;
  gint num_connections;
  gint listen_backlog;
  GList *connections;
  SocketOptions *socket_options;
  TransportMapper *transport_mapper;

  /*
   * Apply transport options, set up bind_addr based on the
   * information processed during parse time. This used to be
   * constructed during the parser, however that made the ordering of
   * various options matter and behave incorrectly when the port() was
   * specified _after_ transport(). Now, it collects the information,
   * and then applies them with a separate call to apply_transport()
   * during init().
   */

  gboolean (*setup_addresses)(AFSocketSourceDriver *s);

  /* optionally acquire a socket from the runtime environment (e.g. systemd) */
  gboolean (*acquire_socket)(AFSocketSourceDriver *s, gint *fd);
};

void afsocket_sd_set_keep_alive(LogDriver *self, gint enable);
void afsocket_sd_set_max_connections(LogDriver *self, gint max_connections);
void afsocket_sd_set_listen_backlog(LogDriver *self, gint listen_backlog);

static inline gboolean
afsocket_sd_acquire_socket(AFSocketSourceDriver *s, gint *fd)
{
  if (s->acquire_socket)
    return s->acquire_socket(s, fd);
  *fd = -1;
  return TRUE;
}

static inline gboolean
afsocket_sd_setup_addresses(AFSocketSourceDriver *s)
{
  return s->setup_addresses(s);
}

gboolean afsocket_sd_setup_addresses_method(AFSocketSourceDriver *self);

gboolean afsocket_sd_init_method(LogPipe *s);
gboolean afsocket_sd_deinit_method(LogPipe *s);
void afsocket_sd_free_method(LogPipe *self);

void afsocket_sd_init_instance(AFSocketSourceDriver *self, SocketOptions *socket_options,
                               TransportMapper *transport_mapper, GlobalConfig *cfg);

#endif
