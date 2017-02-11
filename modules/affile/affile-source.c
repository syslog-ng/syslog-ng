/*
 * Copyright (c) 2002-2014 Balabit
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
#include "affile-source.h"
#include "driver.h"
#include "messages.h"
#include "gprocess.h"
#include "mainloop.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

#include <iv.h>

#define DEFAULT_SD_OPEN_FLAGS (O_RDONLY | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)
#define DEFAULT_SD_OPEN_FLAGS_PIPE (O_RDWR | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)

static inline gboolean
affile_is_device_node(const gchar *filename)
{
  struct stat st;

  if (stat(filename, &st) < 0)
    return FALSE;
  return !S_ISREG(st.st_mode);
}

static inline const gchar *
affile_sd_format_persist_name(const LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *)s;
  return log_pipe_get_persist_name(&self->file_reader->super);
}

static void
affile_sd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  log_src_driver_queue_method(s, msg, path_options, user_data);
}

static gboolean
_are_multi_line_settings_invalid(AFFileSourceDriver *self)
{
  gboolean is_garbage_mode = self->file_reader_options.multi_line_mode == MLM_PREFIX_GARBAGE;
  gboolean is_suffix_mode = self->file_reader_options.multi_line_mode == MLM_PREFIX_SUFFIX;

  return (!is_garbage_mode && !is_suffix_mode) && (self->file_reader_options.multi_line_prefix
         || self->file_reader_options.multi_line_garbage);
}

static gboolean
affile_sd_init(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_src_driver_init_method(s))
    return FALSE;

  log_reader_options_init(&self->file_reader_options.reader_options, cfg, self->super.super.group);

  if (_are_multi_line_settings_invalid(self))
    {
      msg_error("multi-line-prefix() and/or multi-line-garbage() specified but multi-line-mode() is not regexp based "
                "(prefix-garbage or prefix-suffix), please set multi-line-mode() properly");
      return FALSE;
    }

  log_pipe_append(&self->file_reader->super, &self->super.super.super);
  return log_pipe_init(&self->file_reader->super);
}


static gboolean
affile_sd_deinit(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  log_pipe_deinit(&self->file_reader->super);
  if (!log_src_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

static void
affile_sd_free(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  log_pipe_unref(&self->file_reader->super);
  g_string_free(self->filename, TRUE);
  file_reader_options_destroy(&self->file_reader_options);
  log_src_driver_free(s);
}

static AFFileSourceDriver *
affile_sd_new_instance(gchar *filename, GlobalConfig *cfg)
{
  AFFileSourceDriver *self = g_new0(AFFileSourceDriver, 1);

  log_src_driver_init_instance(&self->super, cfg);
  self->filename = g_string_new(filename);
  self->file_reader = file_reader_new(filename, &self->super, cfg);
  self->file_reader->file_reader_options = &self->file_reader_options;
  self->super.super.super.init = affile_sd_init;
  self->super.super.super.queue = affile_sd_queue;
  self->super.super.super.deinit = affile_sd_deinit;
  self->super.super.super.free_fn = affile_sd_free;
  self->super.super.super.generate_persist_name = affile_sd_format_persist_name;
  log_reader_options_defaults(&self->file_reader_options.reader_options);
  file_perm_options_defaults(&self->file_reader_options.file_perm_options);
  self->file_reader_options.reader_options.parse_options.flags |= LP_LOCAL;

  if (affile_is_linux_proc_kmsg(filename))
    self->file_reader_options.file_open_options.needs_privileges = TRUE;
  return self;
}

LogDriver *
affile_sd_new(gchar *filename, GlobalConfig *cfg)
{
  AFFileSourceDriver *self = affile_sd_new_instance(filename, cfg);

  self->file_reader_options.file_open_options.is_pipe = FALSE;
  self->file_reader_options.file_open_options.open_flags = DEFAULT_SD_OPEN_FLAGS;

  if (cfg_is_config_version_older(cfg, 0x0300))
    {
      msg_warning_once("WARNING: file source: default value of follow_freq in file sources has changed in " VERSION_3_0
                       " to '1' for all files except /proc/kmsg");
      self->file_reader_options.follow_freq = -1;
    }
  else
    {
      if (affile_is_device_node(filename) || affile_is_linux_proc_kmsg(filename))
        self->file_reader_options.follow_freq = 0;
      else
        self->file_reader_options.follow_freq = 1000;
    }

  return &self->super.super;
}

LogDriver *
afpipe_sd_new(gchar *filename, GlobalConfig *cfg)
{
  AFFileSourceDriver *self = affile_sd_new_instance(filename, cfg);

  self->file_reader_options.file_open_options.is_pipe = TRUE;
  self->file_reader_options.file_open_options.open_flags = DEFAULT_SD_OPEN_FLAGS_PIPE;

  if (cfg_is_config_version_older(cfg, 0x0302))
    {
      msg_warning_once("WARNING: the expected message format is being changed for pipe() to improve "
                       "syslogd compatibity with " VERSION_3_2 ". If you are using custom "
                       "applications which bypass the syslog() API, you might "
                       "need the 'expect-hostname' flag to get the old behaviour back");
    }
  else
    {
      self->file_reader_options.reader_options.parse_options.flags &= ~LP_EXPECT_HOSTNAME;
    }

  return &self->super.super;
}
