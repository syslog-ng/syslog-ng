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

#include "directory-monitor-content-comparator.h"
#include "messages.h"
#include "timeutils/misc.h"
#include "glib.h"
#include "glib/gstdio.h"

#include <iv.h>


static void
_handle_new_entry(CollectionComparatorEntry *entry, gpointer user_data)
{
  DirectoryMonitorContentComparator *self = (DirectoryMonitorContentComparator *)user_data;
  if (self->super.callback)
    {
      DirectoryMonitorEvent event;

      event.name = entry->value;
      event.full_path = build_filename(self->super.real_path, event.name);
      event.event_type = g_file_test(event.full_path, G_FILE_TEST_IS_DIR) ? DIRECTORY_CREATED : FILE_CREATED;

      self->super.callback(&event, self->super.callback_data);

      g_free(event.full_path);
    }
}

static void
_handle_deleted_entry(CollectionComparatorEntry *entry, gpointer user_data)
{
  DirectoryMonitorContentComparator *self = (DirectoryMonitorContentComparator *)user_data;
  if (self->super.callback)
    {
      DirectoryMonitorEvent event;

      event.name = entry->value;
      event.event_type = FILE_DELETED;
      event.full_path = build_filename(self->super.real_path, event.name);

      self->super.callback(&event, self->super.callback_data);

      g_free(event.full_path);
    }
}

static void
_handle_deleted_self(DirectoryMonitorContentComparator *self)
{
  if (self->super.callback)
    {
      DirectoryMonitorEvent event;

      event.name = self->super.real_path;
      event.event_type = DIRECTORY_DELETED;
      event.full_path = self->super.real_path;

      self->super.callback(&event, self->super.callback_data);
    }
}

void
directory_monitor_content_comparator_rescan_directory(DirectoryMonitorContentComparator *self, gboolean initial_scan)
{
  GError *error = NULL;
  GDir *directory = g_dir_open(self->super.real_path, 0, &error);

  if (FALSE == initial_scan)
    collection_comparator_start(self->comparator);

  if (directory)
    {
      const gchar *filename;
      while((filename = g_dir_read_name(directory)))
        {
          gchar *full_filename = build_filename(self->super.real_path, filename);
          GStatBuf file_stat;

          if (g_stat(full_filename, &file_stat) == 0)
            {
              g_free(full_filename);
              gint64 fids[2] = { file_stat.st_dev, file_stat.st_ino };
              if (initial_scan)
                collection_comparator_add_initial_value(self->comparator, fids, filename);
              else
                collection_comparator_add_value(self->comparator, fids, filename);
            }
          else
            {
              g_free(full_filename);
              msg_error("Error invoking g_stat() on file", evt_tag_str("filename", filename));
            }
        }
      g_dir_close(directory);

      if (FALSE == initial_scan)
        collection_comparator_stop(self->comparator);
    }
  else
    {
      if (FALSE == initial_scan)
        collection_comparator_stop(self->comparator);

      _handle_deleted_self(self);
      msg_debug("Error while opening directory",
                evt_tag_str("dirname", self->super.real_path),
                evt_tag_str("error", error->message));
      g_clear_error(&error);
    }
}

static void
_free_fn(DirectoryMonitor *s)
{
  DirectoryMonitorContentComparator *self = (DirectoryMonitorContentComparator *)s;
  directory_monitor_content_comparator_free(self);
}

void
directory_monitor_content_comparator_free(DirectoryMonitorContentComparator *self)
{
  if (self->comparator)
    collection_comparator_free(self->comparator);
}

void
directory_monitor_content_comparator_init_instance(DirectoryMonitorContentComparator *self,
                                                   const gchar *dir, guint recheck_time,
                                                   const gchar *method)
{
  self->super.free_fn = _free_fn;

  directory_monitor_init_instance(&self->super, dir, recheck_time, method);

  self->comparator = collection_comparator_new();
  collection_comporator_set_callbacks(self->comparator,
                                      _handle_new_entry,
                                      _handle_deleted_entry,
                                      self);
}

DirectoryMonitor *
directory_monitor_content_comparator_new(void)
{
  DirectoryMonitorContentComparator *self = g_new0(DirectoryMonitorContentComparator, 1);
  return &self->super;
}
