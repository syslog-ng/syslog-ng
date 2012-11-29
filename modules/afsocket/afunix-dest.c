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

#include "afunix-dest.h"
#include "misc.h"
#include "messages.h"
#include "gprocess.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

static gboolean
afunix_dd_apply_transport(AFSocketDestDriver *s)
{
  AFUnixDestDriver *self = (AFUnixDestDriver *) s;

  if (self->super.sock_type == SOCK_DGRAM)
    afsocket_dd_set_transport(&self->super.super.super, "unix-dgram");
  else
    afsocket_dd_set_transport(&self->super.super.super, "unix-stream");

  if (!self->super.bind_addr)
    self->super.bind_addr = g_sockaddr_unix_new(NULL);

  if (!self->super.dest_addr)
    self->super.dest_addr = g_sockaddr_unix_new(self->filename);

  if (!self->super.dest_name)
    self->super.dest_name = g_strdup_printf("localhost.afunix:%s", self->filename);
  return TRUE;
}

static void
afunix_dd_free(LogPipe *s)
{
  AFUnixDestDriver *self = (AFUnixDestDriver *) s;

  g_free(self->filename);
  afsocket_dd_free(s);
}

LogDriver *
afunix_dd_new(gint sock_type, gchar *filename)
{
  AFUnixDestDriver *self = g_new0(AFUnixDestDriver, 1);

  afsocket_dd_init_instance(&self->super, &self->sock_options, AF_UNIX, sock_type, "localhost");
  self->super.super.super.super.free_fn = afunix_dd_free;
  self->super.apply_transport = afunix_dd_apply_transport;
  self->super.writer_options.mark_mode = MM_NONE;

  self->filename = g_strdup(filename);

  return &self->super.super.super;
}
