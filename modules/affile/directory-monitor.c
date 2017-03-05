/*
 * Copyright (c) 2017 Balabit
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

#include "directory-monitor.h"

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <messages.h>

static gchar *
build_filename(const gchar *basedir, const gchar *path)
{
  gchar *result;

  if (!path)
    return NULL;

  if (basedir)
    {
      result = (gchar *)g_malloc(strlen(basedir) + strlen(path) + 2);
      sprintf(result, "%s/%s", basedir, path);
    }
  else
    {
      result = g_strdup(path);
    }

  return result;
}

static inline long
get_path_max()
{
  long path_max;
#ifdef PATH_MAX
  path_max = PATH_MAX;
#else
  path_max = pathconf(path, _PC_PATH_MAX);
  if (path_max <= 0)
    path_max = 4096;
#endif
  return path_max;
}

/*
 Resolve . and ..
 Resolve symlinks
 Resolve tricki symlinks like a -> ../a/../a/./b
*/
gchar *
resolve_to_absolute_path(const gchar *path, const gchar *basedir)
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
          msg_error("Can't resolve to absolute path", evt_tag_str("path", path), evt_tag_errno("error", errno), NULL);
          res = NULL;
        }
    }
  g_free(w_name);
  return res;
}

static gchar *
_get_real_path(DirectoryMonitor *self)
{
  gchar *dir_real_path = NULL;
  if (!g_path_is_absolute(self->dir))
    {
      gchar *wd = g_get_current_dir();
      dir_real_path = resolve_to_absolute_path(self->dir, wd);
      g_free(wd);
    }
  else
    {
      dir_real_path = resolve_to_absolute_path(self->dir, NULL);
    }
  return dir_real_path;
}

void
directory_monitor_collect_all_files(DirectoryMonitor *self)
{
  GError *error = NULL;
  gchar *real_path = _get_real_path(self);
  GDir *directory = g_dir_open(real_path, 0, &error);
  if (!directory)
    {
      msg_error("Can not open directory",
                evt_tag_str("base_dir", real_path),
                evt_tag_str("error", error->message));
      g_error_free(error);
      g_free(real_path);
      return;
    }
  const gchar *filename = g_dir_read_name(directory);
  while(filename)
    {
      DirectoryMonitorEvent event = {.name = filename };
      gchar *filename_real_path = resolve_to_absolute_path(filename, real_path);
      event.full_path = build_filename(real_path, filename);
      event.file_type = g_file_test(filename_real_path, G_FILE_TEST_IS_DIR) ? FILE_IS_DIRECTORY : FILE_IS_REGULAR;
      self->callback(&event, self->callback_data);
      g_free(filename_real_path);
      g_free(event.full_path);
      filename = g_dir_read_name(directory);
    }
  g_free(real_path);
  g_dir_close(directory);
}

void
directory_monitor_set_callback(DirectoryMonitor *self, DirectoryMonitorEventCallback callback, gpointer user_data)
{
  self->callback = callback;
  self->callback_data = user_data;
}

DirectoryMonitor *
directory_monitor_new(const gchar *dir)
{
  DirectoryMonitor *self = g_new0(DirectoryMonitor, 1);
  self->dir = g_strdup(dir);
  return self;
}

void
directory_monitor_free(DirectoryMonitor *self)
{
  if (self)
    {
      g_free(self->dir);
      g_free(self);
    }
}
