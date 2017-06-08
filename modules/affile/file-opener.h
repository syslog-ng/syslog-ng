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

#ifndef AFFILE_FILE_OPENER_H_INCLUDED
#define AFFILE_FILE_OPENER_H_INCLUDED

/* portable largefile support for affile */
#include "compat/lfs.h"

#include "file-perms.h"
#include <string.h>

typedef struct _FileOpenOptions
{
  FilePermOptions file_perm_options;
  gboolean needs_privileges:1,
           is_pipe:1;
  gint open_flags;
  gint create_dirs;
} FileOpenOptions, FileOpenerOptions;

gboolean affile_open_file(gchar *name, FileOpenOptions *open_opts, gint *fd);

static inline gboolean
affile_is_linux_proc_kmsg(const gchar *filename)
{
#ifdef __linux__
  if (strcmp(filename, "/proc/kmsg") == 0)
    return TRUE;
#endif
  return FALSE;
}

void file_opener_options_defaults(FileOpenerOptions *options);
void file_opener_options_init(FileOpenerOptions *options, GlobalConfig *cfg);
void file_opener_options_deinit(FileOpenerOptions *options);


#endif
