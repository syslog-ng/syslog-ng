/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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

#include "afunix-source.h"
#include "misc.h"
#include "messages.h"
#include "gprocess.h"

#if ENABLE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#if ENABLE_SYSTEMD
static gboolean
afunix_sd_acquire_named_socket(AFSocketSourceDriver *s, gint *result_fd,
                               const gchar *filename)
{
  AFUnixSourceDriver *self = (AFUnixSourceDriver *) s;
  gint fd, fds;

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
	  /* check if any type is available */
	  if (sd_is_socket_unix(fd, 0, -1, filename, 0))
	    {
	      /* check if it matches our idea of the socket type */
              if (sd_is_socket_unix(fd, self->super.sock_type, -1, filename, 0))
                {
                  *result_fd = fd;
                  break;
                }
              else
                {
                  msg_error("The systemd supplied UNIX domain socket is of a different type, check the configured driver and the matching systemd unit file",
		            evt_tag_str("filename", filename),
		            evt_tag_int("systemd-sock-fd", fd),
                            evt_tag_str("expecting", self->super.sock_type == SOCK_STREAM ? "unix-stream()" : "unix-dgram()"),
                            NULL);
                  return FALSE;
                }
            }
          else
            {

              /* systemd passed an fd we didn't really care about. This is
               * not an error, but might be worth mentioning it at the debug
               * level.
               */

              msg_debug("Ignoring systemd supplied fd as it is not a UNIX domain socket",
		        evt_tag_str("filename", filename),
		        evt_tag_int("systemd-sock-fd", fd),
		        NULL);
            }
	}
    }

  if (*result_fd != -1)
    {
      g_fd_set_nonblock(*result_fd, TRUE);
      g_fd_set_cloexec(*result_fd, TRUE);
      msg_verbose("Acquired systemd socket",
		  evt_tag_str("filename", filename),
		  evt_tag_int("systemd-sock-fd", *result_fd),
		  NULL);
      return TRUE;
    }
  return TRUE;
}

static gboolean
afunix_sd_acquire_socket(AFSocketSourceDriver *s, gint *result_fd)
{
  AFUnixSourceDriver *self = (AFUnixSourceDriver *) s;
  gboolean fd_ok;
  GlobalConfig *cfg = log_pipe_get_config(&s->super.super.super);

  fd_ok = afunix_sd_acquire_named_socket(s, result_fd, self->filename);

  if (fd_ok && (*result_fd == -1) && (strcmp(self->filename, "/dev/log") == 0))
    {
      fd_ok = afunix_sd_acquire_named_socket(s, result_fd, "/run/systemd/journal/syslog");

      if (fd_ok && *result_fd > -1)
        {
          if (cfg_is_config_version_older(cfg, 0x0304))
            {
              msg_warning("WARNING: systemd detected while using /dev/log; migrating automatically to /run/systemd/journal/syslog. Please update your configuration to use the system() source.",
                          evt_tag_str("id", self->super.super.super.id),
                          NULL);

              g_free(self->filename);
              self->filename = g_strdup("/run/systemd/journal/syslog");
              return TRUE;
            }
        }
    }

  if (!fd_ok)
    msg_debug("Failed to acquire systemd socket, trying to open ourselves",
              evt_tag_str("filename", self->filename),
              NULL);

  return fd_ok;
}

#else

static gboolean
afunix_sd_acquire_socket(AFSocketSourceDriver *s, gint *result_fd)
{
  return TRUE;
}
#endif

static gboolean
afunix_sd_apply_transport(AFSocketSourceDriver *s)
{
  AFUnixSourceDriver *self = (AFUnixSourceDriver *) s;

  if (!self->super.bind_addr)
    self->super.bind_addr = g_sockaddr_unix_new(self->filename);

  if (self->super.sock_type == SOCK_DGRAM)
    {
      afsocket_sd_set_transport(&self->super.super.super, "unix-dgram");
      self->super.logproto_name = "dgram";
    }
  else
    {
      afsocket_sd_set_transport(&self->super.super.super, "unix-stream");
      self->super.logproto_name = "text";
    }

  return TRUE;
}

static gboolean
afunix_sd_init(LogPipe *s)
{
  AFUnixSourceDriver *self = (AFUnixSourceDriver *) s;

  if (afsocket_sd_init(s))
    {
      cap_t saved_caps;

      saved_caps = g_process_cap_save();
      g_process_cap_modify(CAP_CHOWN, TRUE);
      g_process_cap_modify(CAP_FOWNER, TRUE);
      g_process_cap_modify(CAP_DAC_OVERRIDE, TRUE);
      file_perm_options_apply_file(&self->file_perm_options, self->filename);
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
afunix_sd_new(gint sock_type, gchar *filename)
{
  AFUnixSourceDriver *self = g_new0(AFUnixSourceDriver, 1);

  afsocket_sd_init_instance(&self->super, &self->sock_options, AF_UNIX, sock_type);

  self->super.super.super.super.init = afunix_sd_init;
  self->super.super.super.super.free_fn = afunix_sd_free;
  self->super.acquire_socket = afunix_sd_acquire_socket;
  self->super.apply_transport = afunix_sd_apply_transport;

  self->super.max_connections = 256;
  self->super.recvd_messages_are_local = TRUE;

  if (self->super.sock_type == SOCK_STREAM)
    self->super.reader_options.super.init_window_size = self->super.max_connections * 100;

  self->filename = g_strdup(filename);
  file_perm_options_defaults(&self->file_perm_options);
  self->file_perm_options.file_perm = 0666;
  return &self->super.super.super;
}
