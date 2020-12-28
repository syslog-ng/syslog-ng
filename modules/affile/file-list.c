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
#include "compat/glib.h"

struct _PendingFileList
{
  GHashTable *index_storage;
  GQueue *priority_queue;
};

PendingFileList *pending_file_list_new(void)
{
  PendingFileList *self = g_new(PendingFileList, 1);
  self->index_storage = g_hash_table_new(g_str_hash, g_str_equal);
  self->priority_queue = g_queue_new();
  return self;
}

void pending_file_list_free(PendingFileList *self)
{
  g_hash_table_unref(self->index_storage);
  g_queue_free_full(self->priority_queue, g_free);
  g_free(self);
}

void pending_file_list_add(PendingFileList *self, const gchar *value)
{
  GList *element = g_hash_table_lookup(self->index_storage, value);
  if (!element)
    {
      gchar *new_value = g_strdup(value);
      g_queue_push_tail(self->priority_queue, new_value);
      g_hash_table_insert(self->index_storage, new_value, self->priority_queue->tail);
    }
}

gchar *pending_file_list_pop(PendingFileList *self)
{
  GList *it = pending_file_list_begin(self);
  if (it == pending_file_list_end(self))
    return NULL;

  gchar *data = it->data;
  pending_file_list_steal(self, it);
  g_list_free_1(it);

  return data;
}

gboolean pending_file_list_remove(PendingFileList *self, const gchar *value)
{
  gboolean is_deleted = FALSE;
  GList *element = g_hash_table_lookup(self->index_storage, value);
  if (element)
    {
      g_hash_table_steal(self->index_storage, element->data);
      g_free(element->data);
      g_queue_delete_link(self->priority_queue, element);
      is_deleted = TRUE;
    }
  return is_deleted;
}

void pending_file_list_steal(PendingFileList *self, GList *entry)
{
  if (!entry) return;

  GList *element = g_hash_table_lookup(self->index_storage, entry->data);
  g_assert(element);

  g_hash_table_steal(self->index_storage, element->data);
  g_queue_unlink(self->priority_queue, entry);
}

GList *pending_file_list_begin(PendingFileList *self)
{
  return self->priority_queue->head;
}

GList *pending_file_list_end(PendingFileList *self)
{
  return NULL;
}

GList *pending_file_list_next(GList *it)
{
  return it->next;
}
