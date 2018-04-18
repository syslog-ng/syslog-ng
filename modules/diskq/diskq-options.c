/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 2016 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "diskq-options.h"
#include "syslog-ng.h"
#include "messages.h"
#include "reloc.h"

void
disk_queue_options_qout_size_set(DiskQueueOptions *self, gint qout_size)
{
  if (qout_size < 64)
    {
      msg_warning("WARNING: The configured qout size is smaller than the minimum allowed",
                  evt_tag_int("configured_size", qout_size),
                  evt_tag_int("minimum_allowed_size", 64),
                  evt_tag_int("new_size", 64));
      qout_size = 64;
    }
  self->qout_size = qout_size;
}

void
disk_queue_options_disk_buf_size_set(DiskQueueOptions *self, gint64 disk_buf_size)
{
  if (disk_buf_size < MIN_DISK_BUF_SIZE)
    {
      msg_warning("WARNING: The configured disk buffer size is smaller than the minimum allowed",
                  evt_tag_int("configured_size", disk_buf_size),
                  evt_tag_int("minimum_allowed_size", MIN_DISK_BUF_SIZE),
                  evt_tag_int("new_size", MIN_DISK_BUF_SIZE));
      disk_buf_size = MIN_DISK_BUF_SIZE;
    }
  self->disk_buf_size = disk_buf_size;
}

void
disk_queue_options_reliable_set(DiskQueueOptions *self, gboolean reliable)
{
  self->reliable = reliable;
}

void
disk_queue_options_mem_buf_size_set(DiskQueueOptions *self, gint mem_buf_size)
{
  self->mem_buf_size = mem_buf_size;
}

void
disk_queue_options_mem_buf_length_set(DiskQueueOptions *self, gint mem_buf_length)
{
  self->mem_buf_length = mem_buf_length;
}

void
disk_queue_options_check_plugin_settings(DiskQueueOptions *self)
{
  if (self->reliable)
    {
      if (self->mem_buf_length > 0)
        {
          msg_warning("WARNING: mem-buf-length parameter was ignored as it is not compatible with reliable queue. Did you mean mem-buf-size?");
        }
    }
  else
    {
      if (self->mem_buf_size > 0)
        {
          msg_warning("WARNING: mem-buf-size parameter was ignored as it is not compatible with non-reliable queue. Did you mean mem-buf-length?");
        }
    }
}

gchar *
_normalize_path(const gchar *path)
{
  const int length = strlen(path);

  if ('/' == path[length-1] || '\\' == path[length-1])
    return g_path_get_dirname(path);

  return g_strdup(path);
}

void
disk_queue_options_set_dir(DiskQueueOptions *self, const gchar *dir)
{
  if (self->dir)
    {
      g_free(self->dir);
    }

  self->dir = _normalize_path(dir);
}

void
disk_queue_options_set_default_options(DiskQueueOptions *self)
{
  self->disk_buf_size = -1;
  self->mem_buf_length = -1;
  self->reliable = FALSE;
  self->mem_buf_size = -1;
  self->qout_size = -1;
  self->dir = g_strdup(get_installation_path_for(SYSLOG_NG_PATH_LOCALSTATEDIR));
}

void
disk_queue_options_destroy(DiskQueueOptions *self)
{
  if (self->dir)
    {
      g_free(self->dir);
      self->dir = NULL;
    }
}


