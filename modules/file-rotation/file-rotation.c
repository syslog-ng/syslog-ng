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
  if(self->last_rotation_time == NULL)
    {
      self->last_rotation_time = g_new(time_t, 1);
      *self->last_rotation_time = time(NULL);
    }
  time_t formatted_date = strftime(NULL, 0, date_format, localtime(self->last_rotation_time));

  GString *formatted_date_string = g_string_new(NULL);

  g_string_printf(formatted_date_string, "%ld", formatted_date);
  msg_debug("Formatted date: %ld", evt_tag_str("formatted_date", formatted_date_string->str));

  self->date_format = g_new(gchar, formatted_date);
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
