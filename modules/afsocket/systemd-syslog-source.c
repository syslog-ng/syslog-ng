/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 2013-2014 Tibor Benke
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


#include "systemd-syslog-source.h"
#include "afunix-source.h"
#include "afsocket-systemd-override.h"
#include "service-management.h"
#include "fdhelpers.h"

#include <stdlib.h>
#include <unistd.h>

#if SYSLOG_NG_ENABLE_SYSTEMD

#include <systemd/sd-daemon.h>

static gboolean
systemd_syslog_sd_acquire_socket(AFSocketSourceDriver *s,
                                 gint *acquired_fd)
{
  gint fd, number_of_fds;

  *acquired_fd = -1;
  fd = -1;

  number_of_fds = sd_listen_fds(0);

  if (number_of_fds > 1)
    {
      msg_error("Systemd socket activation failed: got more than one fd",
                evt_tag_int("number", number_of_fds));

      return TRUE;
    }
  else if (number_of_fds < 1)
    {
      msg_error("Failed to acquire /run/systemd/journal/syslog socket, disabling systemd-syslog source");
      return TRUE;
    }
  else
    {
      fd = SD_LISTEN_FDS_START;
      msg_debug("Systemd socket activation",
                evt_tag_int("file-descriptor", fd));

      if (sd_is_socket_unix(fd, SOCK_DGRAM, -1, NULL, 0))
        {
          *acquired_fd = fd;
        }
      else
        {
          msg_error("The systemd supplied UNIX domain socket is of a"
                    " different type, check the configured driver and"
                    " the matching systemd unit file",
                    evt_tag_int("systemd-sock-fd", fd),
                    evt_tag_str("expecting", "unix-dgram()"));
          *acquired_fd = -1;
          return TRUE;
        }
    }

  if (*acquired_fd != -1)
    {
      g_fd_set_nonblock(*acquired_fd, TRUE);
      msg_verbose("Acquired systemd syslog socket",
                  evt_tag_int("systemd-syslog-sock-fd", *acquired_fd));
      return TRUE;
    }

  return TRUE;
}

#else
static gboolean
systemd_syslog_sd_acquire_socket(AFSocketSourceDriver *s,
                                 gint *acquired_fd)
{
  return TRUE;
}

#endif

static gboolean
systemd_syslog_sd_init_method(LogPipe *s)
{
  SystemDSyslogSourceDriver *self = (SystemDSyslogSourceDriver *) s;

  if (service_management_get_type() != SMT_SYSTEMD)
    {
      msg_error("Error initializing systemd-syslog() source",
                evt_tag_str("systemd_status", "not-running"));
      return FALSE;
    }

  if (self->from_unix_source)
    {
      msg_warning("systemd-syslog() source ignores configuration options. "
                  "Please, do not set anything on it");
      socket_options_free(self->super.socket_options);
      self->super.socket_options = socket_options_new();
      socket_options_init_instance(self->super.socket_options);
    }



  return afsocket_sd_init_method((LogPipe *) &self->super);
}

SystemDSyslogSourceDriver *
systemd_syslog_sd_new(GlobalConfig *cfg, gboolean fallback)
{
  SystemDSyslogSourceDriver *self;
  TransportMapper *transport_mapper;

  self = g_new0(SystemDSyslogSourceDriver, 1);
  transport_mapper = transport_mapper_unix_dgram_new();

  afsocket_sd_init_instance(&self->super, socket_options_new(), transport_mapper, cfg);

  self->super.super.super.super.init = systemd_syslog_sd_init_method;

  self->super.acquire_socket = systemd_syslog_sd_acquire_socket;
  self->super.max_connections = 256;

  if (!self->super.bind_addr)
    self->super.bind_addr = g_sockaddr_unix_new(NULL);

  return self;
}

static gboolean
should_use_systemd_syslog_instead_of_unix_socket(gchar *filename)
{
  return (service_management_get_type() == SMT_SYSTEMD &&
          (strncmp("/dev/log", filename, 9) == 0 || strncmp("/run/systemd/journal/syslog", filename, 28) == 0))? TRUE : FALSE;
}

typedef enum _SocketType
{
  ST_DGRAM,
  ST_STREAM
} SocketType;

AFSocketSourceDriver *
create_afunix_sd(gchar *filename, GlobalConfig *cfg, SocketType socket_type)
{
  AFUnixSourceDriver *ud = NULL;

  if (socket_type == ST_DGRAM)
    {
      ud = afunix_sd_new_dgram(filename, cfg);
    }
  else if (socket_type == ST_STREAM)
    {
      ud = afunix_sd_new_stream(filename, cfg);
    }

  afunix_grammar_set_source_driver(ud);
  return &ud->super;
}

static AFSocketSourceDriver *
create_and_set_unix_socket_or_systemd_syslog_source(gchar *filename, GlobalConfig *cfg, SocketType socket_type)
{
  SystemDSyslogSourceDriver *sd;

  if (should_use_systemd_syslog_instead_of_unix_socket(filename))
    {
      msg_warning("Using /dev/log Unix socket with systemd is not"
                  " possible. Changing to systemd-syslog source, which supports"
                  " socket activation.");

      sd = systemd_syslog_sd_new(configuration, TRUE);
      systemd_syslog_grammar_set_source_driver(sd);
      return &sd->super;
    }
  else return create_afunix_sd(filename, cfg, socket_type);
}

AFSocketSourceDriver *
create_and_set_unix_dgram_or_systemd_syslog_source(gchar *filename, GlobalConfig *cfg)
{
  return create_and_set_unix_socket_or_systemd_syslog_source(filename,
                                                             cfg,
                                                             ST_DGRAM);
}

AFSocketSourceDriver *
create_and_set_unix_stream_or_systemd_syslog_source(gchar *filename, GlobalConfig *cfg)
{
  return create_and_set_unix_socket_or_systemd_syslog_source(filename,
                                                             cfg,
                                                             ST_STREAM);
}
