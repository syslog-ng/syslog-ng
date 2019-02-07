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
#include "timeutils/timeutils.h"

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <messages.h>
#include <iv.h>

gchar *
build_filename(const gchar *basedir, const gchar *path)
{
  gchar *result;

  if (!path)
    return NULL;

  if (basedir)
    {
      result = g_build_path(G_DIR_SEPARATOR_S, basedir, path, NULL);
    }
  else
    {
      result = g_strdup(path);
    }

  return result;
}

#define PATH_MAX_GUESS 1024

static inline long
get_path_max(void)
{
  static long path_max = 0;
  if (path_max == 0)
    {
#ifdef PATH_MAX
      path_max = PATH_MAX;
#else
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
          msg_error("Can't resolve to absolute path",
                    evt_tag_str("path", path),
                    evt_tag_error("error"));
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
directory_monitor_stop(DirectoryMonitor *self)
{
  if (iv_timer_registered(&self->check_timer))
    {
      iv_timer_unregister(&self->check_timer);
    }
  if (iv_task_registered(&self->scheduled_destructor))
    {
      iv_task_unregister(&self->scheduled_destructor);
    }
  if (self->stop_watches && self->watches_running)
    {
      self->stop_watches(self);
    }
  self->watches_running = FALSE;
}

static void
_collect_all_files(DirectoryMonitor *self, GDir *directory)
{
  const gchar *filename = g_dir_read_name(directory);
  while (filename)
    {
      DirectoryMonitorEvent event =
      { .name = filename };
      gchar *filename_real_path = resolve_to_absolute_path(filename, self->real_path);
      event.full_path = build_filename(self->real_path, filename);
      event.event_type = g_file_test(filename_real_path, G_FILE_TEST_IS_DIR) ? DIRECTORY_CREATED : FILE_CREATED;
      self->callback(&event, self->callback_data);
      g_free(filename_real_path);
      g_free(event.full_path);
      filename = g_dir_read_name(directory);
    }
}

static void
_arm_recheck_timer(DirectoryMonitor *self)
{
  iv_validate_now();
  self->check_timer.cookie = self;
  self->check_timer.handler = (GDestroyNotify) directory_monitor_start;
  self->check_timer.expires = iv_now;
  timespec_add_msec(&self->check_timer.expires, self->recheck_time);
  iv_timer_register(&self->check_timer);
}

static void
_set_real_path(DirectoryMonitor *self)
{
  if (self->real_path)
    g_free(self->real_path);
  self->real_path = _get_real_path(self);
}

void
directory_monitor_start(DirectoryMonitor *self)
{
  GDir *directory = NULL;
  GError *error = NULL;
  if (self->watches_running)
    {
      return;
    }
  _set_real_path(self);
  directory = g_dir_open(self->real_path, 0, &error);
  if (!directory)
    {
      msg_error("Can not open directory",
                evt_tag_str("base_dir", self->real_path),
                evt_tag_str("error", error->message));
      _arm_recheck_timer(self);
      g_error_free(error);
      return;
    }
  _collect_all_files(self, directory);
  g_dir_close(directory);
  if (self->start_watches)
    {
      self->start_watches(self);
    }
  self->watches_running = TRUE;
  return;
}

void
directory_monitor_set_callback(DirectoryMonitor *self, DirectoryMonitorEventCallback callback, gpointer user_data)
{
  self->callback = callback;
  self->callback_data = user_data;
}

void
directory_monitor_schedule_destroy(DirectoryMonitor *self)
{
  if (!iv_task_registered(&self->scheduled_destructor))
    {
      iv_task_register(&self->scheduled_destructor);
    }
}

void
directory_monitor_stop_and_destroy(DirectoryMonitor *self)
{
  msg_debug("Stopping directory monitor", evt_tag_str("dir", self->dir));
  directory_monitor_stop(self);
  directory_monitor_free(self);
}

void
directory_monitor_init_instance(DirectoryMonitor *self, const gchar *dir, guint recheck_time)
{
  self->dir = g_strdup(dir);
  self->recheck_time = recheck_time;
  IV_TIMER_INIT(&self->check_timer);
  IV_TASK_INIT(&self->scheduled_destructor);
  self->scheduled_destructor.cookie = self;
  self->scheduled_destructor.handler = (GDestroyNotify)directory_monitor_stop_and_destroy;
}

DirectoryMonitor *
directory_monitor_new(const gchar *dir, guint recheck_time)
{
  DirectoryMonitor *self = g_new0(DirectoryMonitor, 1);
  directory_monitor_init_instance(self, dir, recheck_time);
  return self;
}

void
directory_monitor_free(DirectoryMonitor *self)
{
  if (self)
    {
      msg_debug("Free directory monitor",
                evt_tag_str("dir", self->dir));
      if (self->free_fn)
        {
          self->free_fn(self);
        }
      g_free(self->real_path);
      g_free(self->dir);
      g_free(self);
    }
}
