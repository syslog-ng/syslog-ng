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
disk_queue_options_front_cache_size_set(DiskQueueOptions *self, gint front_cache_size)
{
  self->front_cache_size = front_cache_size;
}

void
disk_queue_options_capacity_bytes_set(DiskQueueOptions *self, gint64 capacity_bytes)
{
  if (capacity_bytes < MIN_CAPACITY_BYTES)
    {
      msg_warning("WARNING: The configured disk buffer capacity is smaller than the minimum allowed",
                  evt_tag_long("configured_capacity", capacity_bytes),
                  evt_tag_long("minimum_allowed_capacity", MIN_CAPACITY_BYTES),
                  evt_tag_long("new_capacity", MIN_CAPACITY_BYTES));
      capacity_bytes = MIN_CAPACITY_BYTES;
    }
  self->capacity_bytes = capacity_bytes;
}

void
disk_queue_options_reliable_set(DiskQueueOptions *self, gboolean reliable)
{
  self->reliable = reliable;
}

void
disk_queue_options_compaction_set(DiskQueueOptions *self, gboolean compaction)
{
  self->compaction = compaction;
}

void
disk_queue_options_flow_control_window_bytes_set(DiskQueueOptions *self, gint flow_control_window_bytes)
{
  self->flow_control_window_bytes = flow_control_window_bytes;
}

void
disk_queue_options_flow_control_window_size_set(DiskQueueOptions *self, gint flow_control_window_size)
{
  self->flow_control_window_size = flow_control_window_size;
}

void
disk_queue_options_set_truncate_size_ratio(DiskQueueOptions *self, gdouble truncate_size_ratio)
{
  self->truncate_size_ratio = truncate_size_ratio;
}

void
disk_queue_options_set_prealloc(DiskQueueOptions *self, gboolean prealloc)
{
  self->prealloc = prealloc;
}

void
disk_queue_options_check_plugin_settings(DiskQueueOptions *self)
{
  if (self->reliable)
    {
      if (self->flow_control_window_size > 0)
        {
          msg_warning("WARNING: flow-control-window-size/mem-buf-length parameter was ignored as it is not compatible with reliable queue. Did you mean flow-control-window-bytes?");
        }
    }
  else
    {
      if (self->flow_control_window_bytes > 0)
        {
          msg_warning("WARNING: flow-control-window-bytes/mem-buf-size parameter was ignored as it is not compatible with non-reliable queue. Did you mean flow-control-window-size?");
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
  self->capacity_bytes = -1;
  self->flow_control_window_size = -1;
  self->reliable = FALSE;
  self->flow_control_window_bytes = -1;
  self->front_cache_size = -1;
  self->dir = g_strdup(get_installation_path_for(SYSLOG_NG_PATH_LOCALSTATEDIR));
  self->truncate_size_ratio = -1;
  self->prealloc = -1;
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
