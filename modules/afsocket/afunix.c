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

#include "afunix.h"
#include "misc.h"
#include "messages.h"
#include "gprocess.h"
#include "sd-daemon.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

void
afunix_sd_set_uid(LogDriver *s, gchar *owner)
{
  AFUnixSourceDriver *self = (AFUnixSourceDriver *) s;

  if (!resolve_user(owner, &self->owner))
    msg_error("Error resolving username",
              evt_tag_str("owner", owner),
              NULL);
}

void
afunix_sd_set_gid(LogDriver *s, gchar *group)
{
  AFUnixSourceDriver *self = (AFUnixSourceDriver *) s;

  if (!resolve_group(group, &self->group))
    msg_error("Error resolving group",
              evt_tag_str("group", group),
              NULL);
}

void
afunix_sd_set_perm(LogDriver *s, gint perm)
{
  AFUnixSourceDriver *self = (AFUnixSourceDriver *) s;

  self->perm = perm & 0777;
}

#if ENABLE_SYSTEMD
static gboolean
afunix_sd_acquire_socket(AFSocketSourceDriver *s, gint *result_fd)
{
  AFUnixSourceDriver *self = (AFUnixSourceDriver *) s;
  gint fd, fds, t, r;

  *result_fd = -1;
  fd = -1;
  fds = sd_listen_fds(0);

  if (fds == 0)
    return TRUE;

  msg_debug("Systemd socket activation",
	    evt_tag_int("systemd-sockets", fds),
	    evt_tag_str("systemd-listen-pid", getenv("LISTEN_PID")),
	    evt_tag_str("systemd-listen-fds", getenv("LISTEN_FDS")),
	    NULL);

  if (fds < 0)
    {
      msg_error("Failed to acquire systemd sockets, incorrectly set LISTEN_FDS environment variable?",
		NULL);
      return FALSE;
    }
  else if (fds > 0)
    {
      for (fd = SD_LISTEN_FDS_START; fd < SD_LISTEN_FDS_START + fds; fd++)
	{
	  t = (self->super.flags & AFSOCKET_STREAM) ? SOCK_STREAM : SOCK_DGRAM;
	  r = sd_is_socket_unix(fd, t, -1, self->filename, 0);
	  if (r == 1)
	    {
	      *result_fd = fd;
	      break;
	    }
	}
    }
  else
    return TRUE;

  if (*result_fd != -1)
    {
      msg_verbose("Acquired systemd socket",
		  evt_tag_str("filename", self->filename),
		  evt_tag_int("systemd-sock-fd", *result_fd),
		  NULL);
    }
  else
    {
      msg_debug("Failed to acquire systemd socket, opening nevertheless",
		evt_tag_str("filename", self->filename),
		NULL);
    }
  return TRUE;
}

#else

static gboolean
afunix_sd_acquire_socket(AFSocketSourceDriver *s, gint *result_fd)
{
  return TRUE;
}
#endif

static gboolean
afunix_sd_init(LogPipe *s)
{
  AFUnixSourceDriver *self = (AFUnixSourceDriver *) s;
  cap_t saved_caps;

  if (afsocket_sd_init(s))
    {
      saved_caps = g_process_cap_save();
      g_process_cap_modify(CAP_CHOWN, TRUE);
      g_process_cap_modify(CAP_FOWNER, TRUE);
      g_process_cap_modify(CAP_DAC_OVERRIDE, TRUE);

      /* change ownership separately, as chgrp may succeed while chown may not */
      if (self->owner >= 0)
        chown(self->filename, (uid_t) self->owner, -1);
      if (self->group >= 0)
        chown(self->filename, -1, (gid_t) self->group);
      if (self->perm >= 0)
        chmod(self->filename, (mode_t) self->perm);
      g_process_cap_restore(saved_caps);

      return TRUE;
    }
  return FALSE;
}

static void
afunix_sd_free(LogPipe *s)
{
  AFUnixSourceDriver *self = (AFUnixSourceDriver *) s;

  g_free(self->filename);
  afsocket_sd_free(s);
}

LogDriver *
afunix_sd_new(gchar *filename, guint32 flags)
{
  AFUnixSourceDriver *self = g_new0(AFUnixSourceDriver, 1);

  afsocket_sd_init_instance(&self->super, &self->sock_options, flags);
  if (self->super.flags & AFSOCKET_DGRAM)
    self->super.transport = g_strdup("unix-dgram");
  else if (self->super.flags & AFSOCKET_STREAM)
    self->super.transport = g_strdup("unix-stream");
  self->super.max_connections = 256;
  self->super.bind_addr = g_sockaddr_unix_new(filename);
  self->super.super.super.init = afunix_sd_init;
  self->super.super.super.free_fn = afunix_sd_free;
  self->super.acquire_socket = afunix_sd_acquire_socket;
  self->filename = g_strdup(filename);
  self->owner = -1;
  self->group = -1;
  self->perm = 0666;
  return &self->super.super;
}

LogDriver *
afunix_dd_new(gchar *filename, guint flags)
{
  AFUnixDestDriver *self = g_new0(AFUnixDestDriver, 1);

  afsocket_dd_init_instance(&self->super, &self->sock_options, flags, g_strdup("localhost"), g_strdup_printf("localhost.afunix:%s", filename));
  if (self->super.flags & AFSOCKET_DGRAM)
    self->super.transport = g_strdup("unix-dgram");
  else if (self->super.flags & AFSOCKET_STREAM)
    self->super.transport = g_strdup("unix-stream");
  self->super.bind_addr = g_sockaddr_unix_new(NULL);
  self->super.dest_addr = g_sockaddr_unix_new(filename);
  return &self->super.super;
}
