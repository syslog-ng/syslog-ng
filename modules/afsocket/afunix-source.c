/*
 * Copyright (c) 2002-2013 Balabit
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
#include "messages.h"
#include "gprocess.h"
#include "transport-mapper-unix.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

static gboolean
afunix_sd_setup_addresses(AFSocketSourceDriver *s)
{
  AFUnixSourceDriver *self = (AFUnixSourceDriver *) s;

  if (!afsocket_sd_setup_addresses_method(s))
    return FALSE;

  if (self->create_dirs)
    {
      if (!file_perm_options_create_containing_directory(&self->file_perm_options, self->filename))
        return FALSE;
    }

  if (!self->super.bind_addr)
    self->super.bind_addr = g_sockaddr_unix_new(self->filename);

  return TRUE;
}

static gboolean
afunix_sd_adjust_reader_options(AFUnixSourceDriver *self, GlobalConfig *cfg)
{
  self->super.reader_options.parse_options.flags |= LP_LOCAL;
  if (cfg_is_config_version_older(cfg, 0x0302))
    {
      msg_warning_once("WARNING: the expected message format is being changed for unix-domain transports to improve "
                       "syslogd compatibity with " VERSION_3_2 ". If you are using custom "
                       "applications which bypass the syslog() API, you might "
                       "need the 'expect-hostname' flag to get the old behaviour back");
    }
  else
    {
      self->super.reader_options.parse_options.flags &= ~LP_EXPECT_HOSTNAME;
    }
  return TRUE;
}

static gboolean
afunix_sd_apply_perms_to_socket(AFUnixSourceDriver *self)
{
  cap_t saved_caps;

  saved_caps = g_process_cap_save();
  g_process_enable_cap("cap_chown");
  g_process_enable_cap("cap_fowner");
  g_process_enable_cap("cap_dac_override");
  file_perm_options_apply_file(&self->file_perm_options, self->filename);
  g_process_cap_restore(saved_caps);
  return TRUE;
}

static gboolean
afunix_sd_init(LogPipe *s)
{
  AFUnixSourceDriver *self = (AFUnixSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (self->create_dirs == -1)
    self->create_dirs = cfg->create_dirs;

  if (self->pass_unix_credentials == -1)
    self->pass_unix_credentials = cfg->pass_unix_credentials;

  file_perm_options_inherit_dont_change(&self->file_perm_options);
  afunix_sd_set_pass_unix_credentials(self, self->pass_unix_credentials);

  return afsocket_sd_init_method(s) &&
         afunix_sd_apply_perms_to_socket(self);
}

static void
afunix_sd_free(LogPipe *s)
{
  AFUnixSourceDriver *self = (AFUnixSourceDriver *) s;

  g_free(self->filename);
  afsocket_sd_free_method(s);
}

AFUnixSourceDriver *
afunix_sd_new_instance(TransportMapper *transport_mapper, gchar *filename, GlobalConfig *cfg)
{
  AFUnixSourceDriver *self = g_new0(AFUnixSourceDriver, 1);

  afsocket_sd_init_instance(&self->super, socket_options_new(), transport_mapper, cfg);

  self->super.super.super.super.init = afunix_sd_init;
  self->super.super.super.super.free_fn = afunix_sd_free;
  self->super.setup_addresses = afunix_sd_setup_addresses;

  self->super.max_connections = 256;

  self->filename = g_strdup(filename);
  file_perm_options_defaults(&self->file_perm_options);
  file_perm_options_set_file_perm(&self->file_perm_options, 0666);

  self->pass_unix_credentials = -1;
  self->create_dirs = -1;

  afunix_sd_adjust_reader_options(self, cfg);
  return self;
}

void
afunix_sd_set_pass_unix_credentials(AFUnixSourceDriver *self, gboolean pass)
{
  self->pass_unix_credentials = pass;

  transport_mapper_unix_set_pass_unix_credentials(self->super.transport_mapper, pass);
}

void
afunix_sd_set_create_dirs(AFUnixSourceDriver *self, gboolean create_dirs)
{
  self->create_dirs = create_dirs;
}

AFUnixSourceDriver *
afunix_sd_new_dgram(gchar *filename, GlobalConfig *cfg)
{
  return afunix_sd_new_instance(transport_mapper_unix_dgram_new(), filename, cfg);
}

AFUnixSourceDriver *
afunix_sd_new_stream(gchar *filename, GlobalConfig *cfg)
{
  AFUnixSourceDriver *self = afunix_sd_new_instance(transport_mapper_unix_stream_new(), filename, cfg);

  self->super.reader_options.super.init_window_size = self->super.max_connections * 100;
  return self;
}
