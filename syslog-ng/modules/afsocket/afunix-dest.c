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

#include "afunix-dest.h"
#include "messages.h"
#include "gprocess.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

static const gchar *
afunix_dd_get_dest_name(const AFSocketDestDriver *s)
{
  const AFUnixDestDriver *self = (const AFUnixDestDriver *)s;
  static gchar buf[256];

  g_snprintf(buf, sizeof(buf), "localhost.afunix:%s", self->filename);
  return buf;
}

static gboolean
afunix_dd_setup_addresses(AFSocketDestDriver *s)
{
  AFUnixDestDriver *self = (AFUnixDestDriver *) s;

  if (!afsocket_dd_setup_addresses_method(s))
    return FALSE;

  if (!self->super.bind_addr)
    self->super.bind_addr = g_sockaddr_unix_new(NULL);

  if (!self->super.dest_addr)
    self->super.dest_addr = g_sockaddr_unix_new(self->filename);

  return TRUE;
}

static void
afunix_dd_free(LogPipe *s)
{
  AFUnixDestDriver *self = (AFUnixDestDriver *) s;

  g_free(self->filename);
  afsocket_dd_free(s);
}

AFUnixDestDriver *
afunix_dd_new_instance(TransportMapper *transport_mapper, gchar *filename, GlobalConfig *cfg)
{
  AFUnixDestDriver *self = g_new0(AFUnixDestDriver, 1);

  afsocket_dd_init_instance(&self->super, socket_options_new(), transport_mapper, cfg);
  self->super.super.super.super.free_fn = afunix_dd_free;
  self->super.setup_addresses = afunix_dd_setup_addresses;
  self->super.writer_options.mark_mode = MM_NONE;
  self->super.get_dest_name = afunix_dd_get_dest_name;
  self->filename = g_strdup(filename);


  return self;
}

AFUnixDestDriver *
afunix_dd_new_dgram(gchar *filename, GlobalConfig *cfg)
{
  return afunix_dd_new_instance(transport_mapper_unix_dgram_new(), filename, cfg);
}

AFUnixDestDriver *
afunix_dd_new_stream(gchar *filename, GlobalConfig *cfg)
{
  return afunix_dd_new_instance(transport_mapper_unix_stream_new(), filename, cfg);
}
