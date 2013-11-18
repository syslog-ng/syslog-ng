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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

gboolean
affile_open_file(gchar *name, gint flags,
                 const FilePermOptions *perm_options,
                 gboolean create_dirs, gboolean privileged, gboolean is_pipe, gint *fd)
{
  cap_t saved_caps;
  struct stat st;
  int mode;

  if (strstr(name, "../") || strstr(name, "/..")) 
    {
      msg_error("Spurious path, logfile not created",
                evt_tag_str("path", name),
                NULL);
      return FALSE;
    }

  saved_caps = g_process_cap_save();
  if (privileged)
    {
      g_process_cap_modify(CAP_DAC_READ_SEARCH, TRUE);
      g_process_cap_modify(CAP_SYSLOG, TRUE);
    }
  else
    {
      g_process_cap_modify(CAP_DAC_OVERRIDE, TRUE);
    }

  if (create_dirs && perm_options && !file_perm_options_create_containing_directory(perm_options, name))
    {
      g_process_cap_restore(saved_caps);
      return FALSE;
    }

  *fd = -1;
  if (stat(name, &st) >= 0)
    {
      if (is_pipe && !S_ISFIFO(st.st_mode))
        {
          msg_warning("WARNING: you are using the pipe driver, underlying file is not a FIFO, it should be used by file()",
                    evt_tag_str("filename", name),
                    NULL);
        }
      else if (!is_pipe && S_ISFIFO(st.st_mode))
        {
          msg_warning("WARNING: you are using the file driver, underlying file is a FIFO, it should be used by pipe()",
                      evt_tag_str("filename", name),
                      NULL);
        }
    }

  mode = perm_options->file_perm < 0 ? 0600 : perm_options->file_perm;
  *fd = open(name, flags, mode);
  if (is_pipe && *fd < 0 && errno == ENOENT)
    {
      if (mkfifo(name, mode) >= 0)
        *fd = open(name, flags, mode);
    }

  if (*fd != -1)
    {
      g_fd_set_cloexec(*fd, TRUE);
      
      g_process_cap_modify(CAP_CHOWN, TRUE);
      g_process_cap_modify(CAP_FOWNER, TRUE);
      if (perm_options)
        file_perm_options_apply_fd(perm_options, *fd);
    }
  g_process_cap_restore(saved_caps);
  msg_trace("affile_open_file",
            evt_tag_str("path", name),
            evt_tag_int("fd",*fd),
            NULL);

  return *fd != -1;
}
