/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include "misc.h"
#include "dnscache.h"
#include "messages.h"
#include "gprocess.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>

gchar *
find_file_in_path(const gchar *path, const gchar *filename, GFileTest test)
{
  gchar **dirs;
  gchar *fullname = NULL;
  gint i;

  if (filename[0] == '/' || !path)
    {
      if (g_file_test(filename, test))
        return g_strdup(filename);
      return NULL;
    }

  dirs = g_strsplit(path, G_SEARCHPATH_SEPARATOR_S, 0);
  i = 0;
  while (dirs && dirs[i])
    {
      fullname = g_build_filename(dirs[i], filename, NULL);
      if (g_file_test(fullname, test))
        break;
      g_free(fullname);
      fullname = NULL;
      i++;
    }
  g_strfreev(dirs);
  return fullname;
}

static gchar *
str_replace_char(const gchar* str, const gchar from, const gchar to)
{
  gchar *p;
  gchar *ret = g_strdup(str);
  p = ret;
  while (*p)
    {
      if (*p == from)
        *p = to;
      p++;
    }
  return ret;
}

gchar *
__normalize_key(const gchar* buffer)
{
  return str_replace_char(buffer, '-', '_');
}
