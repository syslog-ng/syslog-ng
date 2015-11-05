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
#include "affile-common.h"
#include "messages.h"
#include "gprocess.h"
#include "misc.h"
#include "privileged-linux.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

static const gchar* spurious_paths[] = {"../", "/..", NULL};

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
_path_is_spurious(const gchar *name, const gchar *spurious_paths[])
{
  return _string_contains_fragment(name, spurious_paths);
}

static inline void
_set_fd_permission(FilePermOptions *perm_opts, int fd)
{
  gboolean result G_GNUC_UNUSED;

  if (fd != -1)
    {
      g_fd_set_cloexec(fd, TRUE);

      if (perm_opts)
        {
          PRIVILEGED_CALL(CHANGE_FILE_RIGHTS_CAPS, file_perm_options_apply_fd, result, perm_opts, fd);
        }
    }
}

static inline int
_open_fd(const gchar *name, FileOpenOptions *open_opts, FilePermOptions *perm_opts)
{
  int fd;
  int mode = (perm_opts && (perm_opts->file_perm >= 0))
    ? perm_opts->file_perm : 0600;
  int mkfifo_result;

  PRIVILEGED_CALL(OPEN_CAPS(open_opts), open, fd, name, open_opts->open_flags, mode);

  if (open_opts->is_pipe && fd < 0 && errno == ENOENT)
    {
      PRIVILEGED_CALL(MKFIFO_CAPS, mkfifo, mkfifo_result ,name, mode);
      if (mkfifo_result >= 0)
        {
          PRIVILEGED_CALL(OPEN_CAPS(open_opts), open, fd, name, open_opts->open_flags, mode);
        }
    }

  return fd;
}

static inline void
_validate_file_type(const gchar *name, FileOpenOptions *open_opts)
{
  struct stat st;
  gint stat_result;

  PRIVILEGED_CALL(STAT_CAPS, stat, stat_result, name, &st);
  if (stat_result >= 0)
    {
      if (open_opts->is_pipe && !S_ISFIFO(st.st_mode))
        {
          msg_warning("WARNING: you are using the pipe driver, underlying file is not a FIFO, it should be used by file()",
                      evt_tag_str("filename", name),
                      NULL);
        }
      else if (!open_opts->is_pipe && S_ISFIFO(st.st_mode))
        {
          msg_warning("WARNING: you are using the file driver, underlying file is a FIFO, it should be used by pipe()",
                      evt_tag_str("filename", name),
                      NULL);
        }
    }
}

gboolean
__create_containing_directory_and_set_perm_options(FilePermOptions *perm_opts, gchar *name)
{
  gchar *dirname;
  gint rc;
  struct stat st;
  gboolean result;

  /* check that the directory exists */
  dirname = g_path_get_dirname(name);
  PRIVILEGED_CALL(STAT_CAPS, stat, rc, dirname, &st);
  g_free(dirname);

  if (rc == 0)
    {
      /* directory already exists */
      return TRUE;
    }
  else if (rc < 0 && errno != ENOENT)
    {
      /* some real error occurred */
      return FALSE;
    }

  /* directory does not exist */
  PRIVILEGED_CALL(MKDIR_AND_PERM_RIGHTS_CAPS, file_perm_options_create_containing_directory, result, perm_opts, name);
  return result;
}

gboolean
affile_open_file(gchar *name, FileOpenOptions *open_opts, FilePermOptions *perm_opts, gint *fd)
{
  if (_path_is_spurious(name, spurious_paths))
    {
      msg_error("Spurious path, logfile not created",
                evt_tag_str("path", name),
                NULL);
      return FALSE;
    }

  _validate_file_type(name, open_opts);

  if (open_opts->create_dirs &&
      perm_opts &&
      !__create_containing_directory_and_set_perm_options(perm_opts, name))
    return FALSE;

  *fd = _open_fd(name, open_opts, perm_opts);

  _set_fd_permission(perm_opts, *fd);

  msg_trace("affile_open_file",
            evt_tag_str("path", name),
            evt_tag_int("fd", *fd),
            NULL);

  return (*fd != -1);
}
