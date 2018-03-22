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
#include "file-opener.h"
#include "messages.h"
#include "gprocess.h"
#include "fdhelpers.h"
#include "pathutils.h"
#include "cfg.h"
#include "transport/transport-file.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

static inline gboolean
file_opener_prepare_open(FileOpener *self, const gchar *name)
{
  if (self->prepare_open)
    return self->prepare_open(self, name);
  return TRUE;
}

static inline gint
file_opener_open(FileOpener *self, const gchar *name, gint flags)
{
  return self->open(self, name, flags);
}

static gint
file_opener_get_open_flags_method(FileOpener *self, FileDirection dir)
{
  switch (dir)
    {
    case AFFILE_DIR_READ:
      return O_RDONLY | O_NOCTTY | O_NONBLOCK | O_LARGEFILE;
    case AFFILE_DIR_WRITE:
      return O_WRONLY | O_CREAT | O_NOCTTY | O_NONBLOCK | O_LARGEFILE | O_APPEND;
    default:
      g_assert_not_reached();
    }
}

static inline gint
file_opener_get_open_flags(FileOpener *self, FileDirection dir)
{
  return self->get_open_flags(self, dir);
}

static const gchar *spurious_paths[] = {"../", "/..", NULL};

static inline gboolean
_string_contains_fragment(const gchar *str, const gchar *fragments[])
{
  int i;

  for (i = 0; fragments[i]; i++)
    {
      if (strstr(str, fragments[i]))
        return TRUE;
    }

  return FALSE;
}

static inline gboolean
_is_path_spurious(const gchar *name)
{
  return _string_contains_fragment(name, spurious_paths);
}

static inline gboolean
_obtain_capabilities(FileOpener *self, const gchar *name, cap_t *act_caps)
{
  if (self->options->needs_privileges)
    {
      g_process_cap_modify(CAP_DAC_READ_SEARCH, TRUE);
      g_process_cap_modify(CAP_SYSLOG, TRUE);
    }
  else
    {
      g_process_cap_modify(CAP_DAC_OVERRIDE, TRUE);
    }

  if (self->options->create_dirs &&
      !file_perm_options_create_containing_directory(&self->options->file_perm_options, name))
    {
      return FALSE;
    }

  return TRUE;
}

static inline void
_set_fd_permission(FileOpener *self, int fd)
{
  if (fd != -1)
    {
      g_fd_set_cloexec(fd, TRUE);

      g_process_cap_modify(CAP_CHOWN, TRUE);
      g_process_cap_modify(CAP_FOWNER, TRUE);

      file_perm_options_apply_fd(&self->options->file_perm_options, fd);
    }
}

static int
_open(FileOpener *self, const gchar *name, gint open_flags)
{
  FilePermOptions *perm_opts = &self->options->file_perm_options;
  int fd;
  int mode = (perm_opts && (perm_opts->file_perm >= 0))
             ? perm_opts->file_perm : 0600;

  fd = open(name, open_flags, mode);

  return fd;
}

gboolean
file_opener_open_fd(FileOpener *self, const gchar *name, FileDirection dir, gint *fd)
{
  cap_t saved_caps;

  if (_is_path_spurious(name))
    {
      msg_error("Spurious path, logfile not created",
                evt_tag_str("path", name));
      return FALSE;
    }

  saved_caps = g_process_cap_save();

  if (!_obtain_capabilities(self, name, &saved_caps))
    {
      g_process_cap_restore(saved_caps);
      return FALSE;
    }

  if (!file_opener_prepare_open(self, name))
    return FALSE;

  *fd = file_opener_open(self, name, file_opener_get_open_flags(self, dir));

  if (!is_file_device(name))
    _set_fd_permission(self, *fd);

  g_process_cap_restore(saved_caps);

  msg_trace("affile_open_file",
            evt_tag_str("path", name),
            evt_tag_int("fd", *fd));

  return (*fd != -1);
}

void
file_opener_set_options(FileOpener *self, FileOpenerOptions *options)
{
  self->options = options;
}

void
file_opener_init_instance(FileOpener *self)
{
  self->get_open_flags = file_opener_get_open_flags_method;
  self->open = _open;
}

FileOpener *
file_opener_new(void)
{
  FileOpener *self = g_new0(FileOpener, 1);

  file_opener_init_instance(self);
  return self;
}

void
file_opener_free(FileOpener *self)
{
  g_free(self);
}

void
file_opener_options_defaults(FileOpenerOptions *options)
{
  file_perm_options_defaults(&options->file_perm_options);
  options->create_dirs = -1;
  options->needs_privileges = FALSE;
}

void
file_opener_options_defaults_dont_change_permissions(FileOpenerOptions *options)
{
  file_opener_options_defaults(options);
  file_perm_options_inherit_dont_change(&options->file_perm_options);
}

void
file_opener_options_init(FileOpenerOptions *options, GlobalConfig *cfg)
{
  file_perm_options_inherit_from(&options->file_perm_options, &cfg->file_perm_options);
}

void
file_opener_options_deinit(FileOpenerOptions *options)
{
  /* empty, this function only serves to meet the conventions of *Options */
}
