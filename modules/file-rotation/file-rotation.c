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

static void
_slot_file_rotation(FileRotationPlugin *self, FileFlushSignalData *data)
{
  msg_trace("File rotation signal received", evt_tag_str("filename", data->filename));

  if(data != NULL)
    {
      if(data->size >= self->size)
        {
          msg_debug("File size reached the limit", evt_tag_str("filename", data->filename));
          msg_debug("Sending file rotation signal", evt_tag_str("filename", data->filename));

          time_t now = time(NULL);
          struct tm *tm = localtime(&now);
          gchar *date = g_new0(gchar, 100);

          strftime(date, 100, self->date_format, tm);
          data->last_rotation_time = g_new0(gchar, 100);
          gchar *new_filename = g_strconcat(data->filename, date, NULL);

          data->persist_name = g_strdup(new_filename);
          rename(data->filename, new_filename);

          g_free(new_filename);
          g_strlcpy(data->last_rotation_time, date, 100);

          data->reopen = g_new0(gboolean, 1);
          *data->reopen = TRUE;

          data->size = 0;
        }
    }
}

static gboolean
file_rotation_attach_to_driver(LogDriverPlugin *s, LogDriver *driver)
{
  SignalSlotConnector *ssc = driver->super.signal_slot_connector;

  msg_debug("File rotation plugin has been attached", evt_tag_str("to", driver->id));

  CONNECT(ssc, signal_file_flush, _slot_file_rotation, s);

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
  self->interval = interval;
}

void
file_rotation_set_size(FileRotationPlugin *self, gsize size)
{
  self->size = size;
}

void
file_rotation_set_date_format(FileRotationPlugin *self, gchar *date_format)
{
  if (date_format[0] != '-')
    {
      msg_error("Date format must start with '-'", evt_tag_str("date_format", date_format));
      return;
    }
  else if (date_format[1] == '\0')
    {
      msg_error("Date format must not be empty", evt_tag_str("date_format", date_format));
      return;
    }

  g_free(self->date_format);
  self->date_format = g_strconcat("", date_format, NULL);
  printf("%s", self->date_format);

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
