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

#ifndef SDUNIX_H_INCLUDED
#define SDUNIX_H_INCLUDED

#include "driver.h"
#include "afsocket.h"

typedef struct _AFUnixSourceDriver
{
  AFSocketSourceDriver super;
  gchar *filename;
  uid_t owner;
  gid_t group;
  mode_t perm;
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

