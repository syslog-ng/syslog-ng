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

#ifndef SDUNIX_H_INCLUDED
#define SDUNIX_H_INCLUDED

#include "driver.h"
#include "afsocket.h"

typedef struct _AFUnixSourceDriver
{
  AFSocketSourceDriver super;
  gchar *filename;
  /* deliberately using int type here, as we may not fit into 16 bits (e.g. when using -1) */
  gint owner;
  gint group;
  gint perm;
  SocketOptions sock_options;
} AFUnixSourceDriver;

void afunix_sd_set_uid(LogDriver *self, gchar *owner);
void afunix_sd_set_gid(LogDriver *self, gchar *group);
void afunix_sd_set_perm(LogDriver *self, mode_t perm);

LogDriver *afunix_sd_new(gchar *filename, guint32 flags);

typedef struct _AFUnixDestDriver
{
  AFSocketDestDriver super;
  SocketOptions sock_options;
} AFUnixDestDriver;

LogDriver *afunix_dd_new(gchar *filename, guint flags);

#endif
