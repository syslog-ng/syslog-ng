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


gboolean
g_fd_set_nonblock(int fd, gboolean enable)
{
  int flags;

  if ((flags = fcntl(fd, F_GETFL)) == -1)
    return FALSE;
  if (enable)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;

  if (fcntl(fd, F_SETFL, flags) < 0)
    {
      return FALSE;
    }
  return TRUE;
}

gboolean
g_fd_set_cloexec(int fd, gboolean enable)
{
  int flags;

  if ((flags = fcntl(fd, F_GETFD)) == -1)
    return FALSE;
  if (enable)
    flags |= FD_CLOEXEC;
  else
    flags &= ~FD_CLOEXEC;

  if (fcntl(fd, F_SETFD, flags) < 0)
    {
      return FALSE;
    }
  return TRUE;
}

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

/*
 * NOTE: pointer values below 0x1000 (4096) are taken as special values used
 * by the application code and are not duplicated, but assumed to be literal
 * tokens.
 */
GList *
string_list_clone(GList *string_list)
{
  GList *cloned = NULL;
  GList *l;

  for (l = string_list; l; l = l->next)
    cloned = g_list_append(cloned, GPOINTER_TO_UINT(l->data) > 4096 ? g_strdup(l->data) : l->data);

  return cloned;
}

GList *
string_array_to_list(const gchar *strlist[])
{
  gint i;
  GList *l = NULL;

  for (i = 0; strlist[i]; i++)
    {
      l = g_list_prepend(l, g_strdup(strlist[i]));
    }

  return g_list_reverse(l);
}

/*
 * NOTE: pointer values below 0x1000 (4096) are taken as special
 * values used by the application code and are not freed. Since this
 * is the NULL page, this should not cause memory leaks.
 */
void
string_list_free(GList *l)
{
  while (l)
    {
      /* some of the string lists use invalid pointer values as special
       * items, see SQL "default" item */

      if (GPOINTER_TO_UINT(l->data) > 4096)
        g_free(l->data);
      l = g_list_delete_link(l, l);
    }
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
