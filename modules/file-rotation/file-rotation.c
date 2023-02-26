/*
 * Copyright (c) 2022 Shikhar Vashistha
 * Copyright (c) 2022 László Várady
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

#include "file-rotation.h"
#include "modules/affile/file-signals.h"
#include "driver.h"
#include "time.h"
#include <sys/stat.h>

static void
_slot_file_rotation(FileRotationPlugin *self, FileFlushSignalData *data)
{
  msg_trace("File rotation signal received", evt_tag_str("filename", data->filename));

  g_assert(data);

  if(self->number_of_time_rotated > self->number_of_file_rotations)
    {
      process_file_removal(self, data->filename);
    }

  struct stat st;
  stat(data->filename, &st);

  if(st.st_size > self->size)
    {
      time_t now = time(NULL);
      struct tm *tm = localtime(&now);
      gchar date[101];

      strftime(date, 100, self->date_format, tm);
      gchar *new_filename = g_strdup_printf("%s%s.%ld", data->filename, date, self->number_of_time_rotated);

      msg_info("File size reached the limit, rotating file",
               evt_tag_str("filename", data->filename),
               evt_tag_str("new_filename", new_filename));

      rename(data->filename, new_filename);

      self->file_names = g_list_append(self->file_names, new_filename);
      self->filename= new_filename;

      if(g_file_test(new_filename, G_FILE_TEST_EXISTS))
        {
          process_file_removal(self, new_filename);
        }

      g_free(new_filename);

      gsize max_index = 0;
      for(gsize i = 0; i < self->number_of_file_rotations; i++)
        {
          gchar *filename = g_strdup_printf("%s%s.%ld", data->filename, date, i);
          if(g_file_test(filename, G_FILE_TEST_EXISTS))
            {
              max_index = i;
            }
          g_free(filename);
        }

      self->number_of_time_rotated= max_index + 1;


      file_reopener_request_reopen(data->reopener);
    }

}

void
process_file_removal(FileRotationPlugin *self, const gchar *filename)
{
  gchar *file = g_strdup_printf("%s.%ld", self->filename, self->number_of_file_rotations-1);
  if(remove(file) == 0)
    {
      msg_trace("File deleted successfully", evt_tag_str("filename", file));
    }
  else
    {
      msg_trace("Error: unable to delete the file", evt_tag_str("filename", file));
    }
  g_free(file);
  for(gsize i = self->number_of_file_rotations; i > 0; i--)
    {
      gchar *file_name = g_strdup_printf("%s.%ld", self->filename, i);
      gchar *new_filename = g_strdup_printf("%s.%ld", self->filename, i+1);
      rename(file_name, new_filename);
      g_free(file_name);
      g_free(new_filename);
    }
  self->number_of_file_rotations=0;
  gchar *new_filename = g_strdup_printf("%s.%d", self->filename, 0);
  rename(filename, new_filename);
}

static gboolean
file_rotation_attach_to_driver(LogDriverPlugin *s, LogDriver *driver)
{
  SignalSlotConnector *ssc = driver->super.signal_slot_connector;

  msg_debug("File rotation plugin has been attached", evt_tag_str("to", driver->id));

  CONNECT(ssc, signal_file_flush, _slot_file_rotation, s);

  FileRotationPlugin *self = (FileRotationPlugin *) s;

  //if(self->size == 0)
  //  {
  //    msg_error("File rotation size cannot be zero");
  //    return FALSE;
  //  }

  if(self->number_of_file_rotations == 0)
    {
      msg_error("File rotation number cannot be zero");
      return FALSE;
    }

  return TRUE;

}

static void
file_rotation_free(LogDriverPlugin *s)
{
  msg_debug("FileRotationPlugin::free");
  FileRotationPlugin *self = (FileRotationPlugin *)s;
  g_free(self->filename);
  log_driver_plugin_free_method(s);
}

void
file_rotation_set_interval(FileRotationPlugin *self, gchar *interval)
{
  g_free(self->interval);
  self->interval = g_strdup(interval);
  msg_debug("File rotation interval has been set", evt_tag_str("interval", self->interval));
}

void
file_rotation_set_number_of_file_rotatations(FileRotationPlugin *self, gsize rotate)
{
  self->number_of_file_rotations = rotate;
  self->number_of_time_rotated = 0;
}

void
file_rotation_set_size(FileRotationPlugin *self, gsize size)
{
  self->size = size;
}

void
file_rotation_set_date_format(FileRotationPlugin *self, gchar *date_format)
{
  g_free(self->date_format);
  g_strdup(self->date_format);
  if(self->date_format == NULL)
    {
      self->date_format = g_strdup("");
    }
  self->date_format = g_strconcat("", date_format, NULL);

}

FileRotationPlugin *
file_rotation_new(void)
{
  FileRotationPlugin *self = g_new0(FileRotationPlugin, 1);

  log_driver_plugin_init_instance(&self->super, "file-rotation");

  self->super.attach = file_rotation_attach_to_driver;
  self->super.free_fn = file_rotation_free;

  return self;
}
