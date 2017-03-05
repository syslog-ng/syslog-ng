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
#include "apphook.h"
#include <criterion/criterion.h>
#include <glib/gstdio.h>
#include <unistd.h>

TestSuite(directory_monitor, .init = app_startup, .fini = app_shutdown);

static void
_callback(const DirectoryMonitorEvent *event, gpointer user_data)
{
  GList **p_list = (GList **)user_data;
  if (event->file_type == FILE_IS_REGULAR)
    {
      *p_list = g_list_append(*p_list, g_strdup(event->name));
    }
}

Test(directory_monitor, read_content_of_directory)
{
  gchar *dir_pattern = g_strdup("read_content_of_directoryXXXXXX");
  gchar *tmpdir = g_mkdtemp(dir_pattern);
  cr_assert(tmpdir);
  gchar *file_list[10] = {0};
  gchar *file_list_full_path[10] = {0};
  for (gint i = 0; i < 10; i++)
    {
      GError *error = NULL;
      file_list[i] = g_strdup_printf("file_%d.txt", i);
      file_list_full_path[i] = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", tmpdir, file_list[i]);
      gboolean res = g_file_set_contents(file_list_full_path[i], file_list[i], strlen(file_list[i]), &error);
      cr_assert(res != FALSE, "Error: %s", error ? error->message : "OK");
    }
  DirectoryMonitor *monitor = directory_monitor_new(tmpdir);
  GList *found_files = NULL;
  directory_monitor_set_callback(monitor, _callback, &found_files);
  directory_monitor_collect_all_files(monitor);

  for (gint i = 0; i < 10; i++)
    {
      cr_assert(g_list_find_custom(found_files, file_list[i], (GCompareFunc)strcmp), "Can not find: %s", file_list[i]);
      unlink(file_list_full_path[i]);
      g_free(file_list_full_path[i]);
      g_free(file_list[i]);
    }
  g_list_free_full(found_files, g_free);
  g_rmdir(tmpdir);
  g_free(tmpdir);
  directory_monitor_free(monitor);
}

Test(directory_monitor, non_existing_directory)
{
  DirectoryMonitor *monitor = directory_monitor_new("this directory should not exist");
  GList *found_files = NULL;
  directory_monitor_set_callback(monitor, _callback, &found_files);
  directory_monitor_collect_all_files(monitor);
  cr_assert_null(found_files);
  directory_monitor_free(monitor);
}
