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
#include "messages.h"
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

static inline long
get_path_max(void)
{
  static long path_max = 0;
  if (path_max == 0)
    {
#ifdef PATH_MAX
      path_max = PATH_MAX;
#else
#define PATH_MAX_GUESS 1024
      /* This code based on example from the Advanced Programming in the UNIX environment
       * on how to determine the max path length
       */
      static long posix_version = 0;
      static long xsi_version = 0;
      if (posix_version == 0)
        posix_version = sysconf(_SC_VERSION);

      if (xsi_version == 0)
        xsi_version = sysconf(_SC_XOPEN_VERSION);

      if ((path_max = pathconf("/", _PC_PATH_MAX)) < 0)
        path_max = PATH_MAX_GUESS;
      else
        path_max++;    /* add one since it's relative to root */

      /*
       * Before POSIX.1-2001, we aren't guaranteed that PATH_MAX includes
       * the terminating null byte.  Same goes for XPG3.
       */
      if ((posix_version < 200112L) && (xsi_version < 4))
        path_max++;

#endif
    }
  return path_max;
}

/*
 Resolve . and ..
 Resolve symlinks
 Resolve tricki symlinks like a -> ../a/../a/./b
*/
gchar *
resolve_to_absolute_path(const gchar *basedir, const gchar *path)
{
  long path_max = get_path_max();
  gchar *res;
  gchar *w_name;

  w_name = build_filename(basedir, path);
  res = (char *)g_malloc(path_max);

  if (!realpath(w_name, res))
    {
      g_free(res);
      if (errno == ENOENT)
        {
          res = g_strdup(path);
        }
      else
        {
          msg_error("Can't resolve to absolute path",
                    evt_tag_str("path", path),
                    evt_tag_error("error"));
          res = NULL;
        }
    }
  g_free(w_name);
  return res;
}

gchar *
build_filename(const gchar *basedir, const gchar *path)
{
  if (!path)
    return NULL;

  gchar *result;
  if (basedir)
    result = g_build_path(G_DIR_SEPARATOR_S, basedir, path, NULL);
  else
    result = g_strdup(path);
  return result;
}

gchar *
canonicalize_filename(const gchar *path)
{
  gchar *absolute_path = resolve_to_absolute_path(NULL, path);
  gchar *resolved_path = g_canonicalize_filename(absolute_path, NULL);
  g_free(absolute_path);
  return resolved_path;
}
