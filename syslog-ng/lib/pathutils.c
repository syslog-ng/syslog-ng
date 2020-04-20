/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 2013 Viktor Juhasz
 * Copyright (c) 2013 Viktor Tusa
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

#include "pathutils.h"
#include <sys/stat.h>
#include <string.h>

gboolean
is_file_directory(const char *filename)
{
  return g_file_test(filename, G_FILE_TEST_EXISTS) && g_file_test(filename, G_FILE_TEST_IS_DIR);
};

gboolean
is_file_regular(const char *filename)
{
  return g_file_test(filename, G_FILE_TEST_EXISTS) && g_file_test(filename, G_FILE_TEST_IS_REGULAR);
};

gboolean
is_file_device(const gchar *name)
{
  struct stat st;

  if (stat(name, &st) >= 0)
    return S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode);
  else
    return FALSE;
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

const gchar *
get_filename_extension(const gchar *filename)
{
  if (!filename)
    return NULL;

  const gchar *start_ext = strrchr(filename, '.');

  if (!start_ext || start_ext[1] == '\0' || start_ext == filename)
    return NULL;

  return start_ext + 1;
}
