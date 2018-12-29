/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "collection-comparator.h"

#define IN_OLD   0x01
#define IN_NEW   0x02
#define IN_BOTH  0x03

typedef struct _CollectionComparatorEntry
{
  gchar *value;
  guint8 flag;
} CollectionComparatorEntry;

struct _CollectionComparator
{
  GList *original_list;
  GHashTable *original_map;
  gboolean running;
  GList *deleted_entries;
  gpointer callback_data;

  void (*handle_new_entry)(const gchar *name, gpointer callback_data);
  void (*handle_deleted_entry)(const gchar *name, gpointer callback_data);
};

static void
_free_poll_entry(gpointer s)
{
  CollectionComparatorEntry *entry = (CollectionComparatorEntry *)s;
  g_free(entry->value);
  g_free(entry);
}

void
collection_comparator_free(CollectionComparator *self)
{
  g_hash_table_unref(self->original_map);
  g_list_free_full(self->original_list, _free_poll_entry);
  g_free(self);
}

CollectionComparator *
collection_comparator_new(void)
{
  CollectionComparator *self = g_new0(CollectionComparator, 1);
  self->original_map = g_hash_table_new(g_str_hash, g_str_equal);
  return self;
}

void
collection_comparator_start(CollectionComparator *self)
{
  if (!self->running)
    {
      self->running = TRUE;
      self->deleted_entries = NULL;
    }
}

void
collection_comparator_add_value(CollectionComparator *self, const gchar *value)
{
  CollectionComparatorEntry *entry = g_hash_table_lookup(self->original_map, value);
  if (entry)
    {
      entry->flag = IN_BOTH;
    }
  else
    {
      entry = g_new0(CollectionComparatorEntry, 1);
      entry->value = g_strdup(value);
      entry->flag = IN_NEW;
      self->original_list = g_list_append(self->original_list, entry);
      g_hash_table_insert(self->original_map, entry->value, entry);
      self->handle_new_entry(entry->value, self->callback_data);
    }
}

void
collection_comparator_add_initial_value(CollectionComparator *self, const gchar *value)
{
  CollectionComparatorEntry *entry = g_new0(CollectionComparatorEntry, 1);
  entry->value = g_strdup(value);
  entry->flag = IN_OLD;
  self->original_list = g_list_append(self->original_list, entry);
  g_hash_table_insert(self->original_map, entry->value, entry);
}

void
_move_link(GList *item, GList **old_list, GList **new_list)
{
  *old_list = g_list_remove_link(*old_list, item);
  *new_list = g_list_concat(*new_list, item);
}

void
_deleted_entries_callback(gpointer data, gpointer user_data)
{
  CollectionComparatorEntry *entry = (CollectionComparatorEntry *)data;
  CollectionComparator *self = (CollectionComparator *)user_data;
  self->handle_deleted_entry(entry->value, self->callback_data);
}

void
collection_comparator_collect_deleted_entries(CollectionComparator *self)
{
  GList *iter = self->original_list;
  CollectionComparatorEntry *entry;
  while (iter)
    {
      entry = (CollectionComparatorEntry *)iter->data;
      if (entry->flag == IN_OLD)
        {
          GList *next = iter->next;
          g_hash_table_remove(self->original_map, entry->value);
          _move_link(iter, &self->original_list, &self->deleted_entries);
          iter = next;
        }
      else
        {
          entry->flag = IN_OLD;
          iter = iter->next;
        }
    }
}

void
collection_comparator_stop(CollectionComparator *self)
{
  collection_comparator_collect_deleted_entries(self);
  g_list_foreach(self->deleted_entries, _deleted_entries_callback, self);
  g_list_free_full(self->deleted_entries, _free_poll_entry);
  self->running = FALSE;
}

void
collection_comporator_set_callbacks(CollectionComparator *self, cc_callback handle_new, cc_callback handle_delete,
                                    gpointer user_data)
{
  self->callback_data = user_data;
  self->handle_deleted_entry = handle_delete;
  self->handle_new_entry = handle_new;
}
