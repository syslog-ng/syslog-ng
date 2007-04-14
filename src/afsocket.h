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

#ifndef AFSOCKET_H_INCLUDED
#define AFSOCKET_H_INCLUDED

#include "driver.h"
#include "logreader.h"
#include "logwriter.h"

#define AFSOCKET_DGRAM               0x0001
#define AFSOCKET_STREAM              0x0002
#define AFSOCKET_LOCAL               0x0004

#define AFSOCKET_KEEP_ALIVE          0x0100

typedef enum
{
  AFSOCKET_DIR_RECV = 0x01,
  AFSOCKET_DIR_SEND = 0x02,
} AFSocketDirection;

typedef struct _AFSocketSourceDriver AFSocketSourceDriver;
typedef struct _AFSocketDestDriver AFSocketDestDriver;

typedef struct _SocketOptions
{
  gint sndbuf;
  gint rcvbuf;
  gint broadcast;
} SocketOptions;

gboolean afsocket_setup_socket(gint fd, SocketOptions *sock_options, AFSocketDirection dir);

struct _AFSocketSourceDriver
{
  LogDriver super;
  guint32 flags;
  gint fd;
  guint source_id;
  LogReaderOptions reader_options;

  GSockAddr *bind_addr;
  gint max_connections;
  gint num_connections;
  gint listen_backlog;
  GList *connections;
  SocketOptions *sock_options_ptr;
  gboolean (*setup_socket)(AFSocketSourceDriver *s, gint fd);
};

void afsocket_sd_set_keep_alive(LogDriver *self, gint enable);
void afsocket_sd_set_max_connections(LogDriver *self, gint max_connections);

gboolean afsocket_sd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist);
gboolean afsocket_sd_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist);

void afsocket_sd_init_instance(AFSocketSourceDriver *self, SocketOptions *sock_options, guint32 flags);
void afsocket_sd_free_instance(AFSocketSourceDriver *self);

struct _AFSocketDestDriver
{
  LogDriver super;
  guint32 flags;
  gint fd;
  guint source_id;
  LogPipe *writer;
  LogWriterOptions writer_options;

  GSockAddr *bind_addr;
  GSockAddr *dest_addr;
  gint time_reopen;
  guint reconnect_timer;
  SocketOptions *sock_options_ptr;
  gboolean (*setup_socket)(AFSocketDestDriver *s, gint fd);
};

void afsocket_dd_init_instance(AFSocketDestDriver *self, SocketOptions *sock_options, guint32 flags);


#endif
