/*
 * Copyright (c) 2018 Balabit
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
#include "file-list.h"

struct _FileList
{
  GHashTable *hash_table;
  GQueue *queue;
};

FileList *file_list_new(void)
{
  FileList *self = g_new(FileList, 1);
  self->hash_table = g_hash_table_new(g_str_hash, g_str_equal);
  self->queue = g_queue_new();
  return self;
}

void file_list_free(FileList *self)
{
  g_hash_table_unref(self->hash_table);
  g_queue_free_full(self->queue, g_free);
  g_free(self);
}

void file_list_add(FileList *self, const gchar *value)
{
  GList *element = g_hash_table_lookup(self->hash_table, value);
  if (!element)
    {
      gchar *new_value = g_strdup(value);
      g_queue_push_tail(self->queue, new_value);
      g_hash_table_insert(self->hash_table, new_value, self->queue->tail);
    }
}

gchar *file_list_pop(FileList *self)
{
  gchar *data = g_queue_pop_head(self->queue);
  if (data)
    {
      g_hash_table_steal(self->hash_table, data);
    }
  return data;
}

gboolean file_list_remove(FileList *self, const gchar *value)
{
  gboolean is_deleted = FALSE;
  GList *element = g_hash_table_lookup(self->hash_table, value);
  if (element)
    {
      g_hash_table_steal(self->hash_table, element->data);
      g_free(element->data);
      g_queue_delete_link(self->queue, element);
      is_deleted = TRUE;
    }
  return is_deleted;
}
