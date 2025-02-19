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

#include "collection-comparator.h"

#define IN_OLD   0x01
#define IN_NEW   0x02
#define IN_BOTH  0x03

struct _CollectionComparator
{
  GList *original_list;
  GHashTable *original_map;
  gboolean running;
  GList *deleted_entries;
  GList *added_entries;
  gpointer callback_data;

  cc_callback handle_new_entry;
  cc_callback handle_deleted_entry;
};

inline guint
hash_collection_comparator_entry(const void *data)
{
  const CollectionComparatorEntry *entry = data;
  gint64 hash1 = g_int64_hash(&entry->key[0]);
  gint64 hash2 = g_int64_hash(&entry->key[1]);
  gint64 hash3 = g_str_hash(entry->value);
  guint hash = hash1 ^ hash2 ^ hash3;
  return hash;
}

inline gboolean
equal_collection_comparator_entry(const void *a, const void *b)
{
  CollectionComparatorEntry *entry1 = (CollectionComparatorEntry *) a;
  CollectionComparatorEntry *entry2 = (CollectionComparatorEntry *) b;
  return memcmp(entry1->key, entry2->key, 2 * sizeof(gint64)) == 0 && g_str_equal(entry1->value, entry2->value);
}

inline void
free_collection_comparator_entry(gpointer s)
{
  CollectionComparatorEntry *entry = (CollectionComparatorEntry *)s;
  g_free(entry->value);
  g_free(entry);
}

void
collection_comparator_free(CollectionComparator *self)
{
  g_hash_table_unref(self->original_map);
  g_list_free_full(self->original_list, free_collection_comparator_entry);
  g_free(self);
}

CollectionComparator *
collection_comparator_new(void)
{
  CollectionComparator *self = g_new0(CollectionComparator, 1);
  self->original_map = g_hash_table_new(hash_collection_comparator_entry, equal_collection_comparator_entry);
  return self;
}

void
collection_comparator_start(CollectionComparator *self)
{
  if (!self->running)
    {
      self->running = TRUE;
      self->deleted_entries = NULL;
      self->added_entries = NULL;
    }
}

void
collection_comparator_add_value(CollectionComparator *self, const gint64 key[2], const gchar *value)
{
  CollectionComparatorEntry lookup_entry = { { key[0], key[1] }, (gchar *) value, 0};
  CollectionComparatorEntry *entry = g_hash_table_lookup(self->original_map, &lookup_entry);
  if (entry)
    {
      entry->flag = IN_BOTH;
    }
  else
    {
      entry = g_new0(CollectionComparatorEntry, 1);
      memcpy(entry->key, key, 2 * sizeof(gint64));
      entry->value = g_strdup(value);
      entry->flag = IN_NEW;
      self->original_list = g_list_append(self->original_list, entry);
      g_hash_table_insert(self->original_map, entry, entry);
      self->added_entries = g_list_prepend(self->added_entries, entry);
    }
}

void
collection_comparator_add_initial_value(CollectionComparator *self, const gint64 key[2], const gchar *value)
{
  CollectionComparatorEntry *entry = g_new0(CollectionComparatorEntry, 1);
  memcpy(entry->key, key, 2 * sizeof(gint64));
  entry->value = g_strdup(value);
  entry->flag = IN_OLD;
  self->original_list = g_list_append(self->original_list, entry);
  g_hash_table_insert(self->original_map, entry, entry);
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
  self->handle_deleted_entry(entry, self->callback_data);
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
          g_hash_table_remove(self->original_map, entry);
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
_added_entries_callback(gpointer data, gpointer user_data)
{
  CollectionComparatorEntry *entry = (CollectionComparatorEntry *)data;
  CollectionComparator *self = (CollectionComparator *)user_data;
  self->handle_new_entry(entry, self->callback_data);
}

void
collection_comparator_stop(CollectionComparator *self)
{
  collection_comparator_collect_deleted_entries(self);
  g_list_foreach(self->deleted_entries, _deleted_entries_callback, self);
  g_list_free_full(self->deleted_entries, free_collection_comparator_entry);
  g_list_foreach(self->added_entries, _added_entries_callback, self);
  /* the items in self->added_entries are not owned here */
  g_list_free(self->added_entries);
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
