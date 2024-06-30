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

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include "collection-comparator.h"

#include <glib/gstdio.h>
#include <unistd.h>

typedef struct _TestData
{
  GHashTable *deleted_entries;
  GHashTable *new_entries;
} TestData;

TestData *
test_data_new(void)
{
  TestData *self = g_new0(TestData, 1);
  self->deleted_entries = g_hash_table_new_full(hash_collection_comparator_entry,
                                                equal_collection_comparator_entry,
                                                NULL,
                                                free_collection_comparator_entry);
  self->new_entries = g_hash_table_new_full(hash_collection_comparator_entry,
                                            equal_collection_comparator_entry,
                                            NULL,
                                            free_collection_comparator_entry);
  return self;
}

void test_data_free(TestData *self)
{
  g_hash_table_unref(self->deleted_entries);
  g_hash_table_unref(self->new_entries);
  g_free(self);
}

static void
_handle_new(CollectionComparatorEntry *new_entry, gpointer data)
{
  TestData *self = (TestData *)data;

  CollectionComparatorEntry *entry = g_new0(CollectionComparatorEntry, 1);
  memcpy(entry->key, new_entry->key, 2 * sizeof(gint64));
  entry->value = g_strdup(new_entry->value);
  entry->flag = 0;

  g_hash_table_insert(self->new_entries, entry, entry);
}

static void
_handle_delete(CollectionComparatorEntry *deleted_entry, gpointer data)
{
  TestData *self = (TestData *)data;

  CollectionComparatorEntry *entry = g_new0(CollectionComparatorEntry, 1);
  memcpy(entry->key, deleted_entry->key, 2 * sizeof(gint64));
  entry->value = g_strdup(deleted_entry->value);
  entry->flag = 0;

  g_hash_table_insert(self->deleted_entries, entry, entry);
}

struct TestFileList
{
  gint ndx;
  const gchar *name;
  const CollectionComparatorEntry initial_files[5];
  const CollectionComparatorEntry result_files[5];
  const CollectionComparatorEntry expected_deleted_files[5];
  const CollectionComparatorEntry expected_new_files[5];
};

ParameterizedTestParameters(collection_comparator, test_collection_changes)
{
  static const struct TestFileList params[] =
  {
    {
      0, "nothing_changed",
      {{{1, 1}, "test1", 0}, {{2, 2}, "test2", 0}},
      {{{1, 1}, "test1", 0}, {{2, 2}, "test2", 0}},
      {{{0, 0}, NULL, 0}},
      {{{0, 0}, NULL, 0}}
    },
    {
      1, "first_file_deleted",
      {{{1, 1}, "test1", 0}, {{2, 2}, "test2", 0}},
      {{{2, 2}, "test2", 0}},
      {{{1, 1}, "test1", 0}},
      {{{0, 0}, NULL, 0}}
    },
    {
      2, "last_file_deleted",
      {{{1, 1}, "test1", 0}, {{2, 2}, "test2", 0}},
      {{{1, 1}, "test1", 0}},
      {{{2, 2}, "test2", 0}},
      {{{0, 0}, NULL, 0}}
    },
    {
      3, "deleted_all",
      {{{1, 1}, "test1", 0}, {{2, 2}, "test2", 0}},
      {{{0, 0}, NULL, 0}},
      {{{1, 1}, "test1", 0}, {{2, 2}, "test2", 0}},
      {{{0, 0}, NULL, 0}}
    },
    {
      4, "created_all",
      {{{0, 0}, NULL, 0}},
      {{{1, 1}, "test1", 0}, {{2, 2}, "test2", 0}},
      {{{0, 0}, NULL, 0}},
      {{{1, 1}, "test1", 0}, {{2, 2}, "test2", 0}},
    },
    {
      5, "mixed",
      {{{1, 1}, "test1", 0}, {{2, 2}, "test2", 0}, {{3, 3}, "test3", 0}},
      {{{1, 1}, "test1", 0}, {{4, 4}, "test4", 0}},
      {{{2, 2}, "test2", 0}, {{3, 3}, "test3", 0}},
      {{{4, 4}, "test4", 0}}
    }
  };

  size_t nb_params = sizeof(params) / sizeof(const struct TestFileList);
  return cr_make_param_array(const struct TestFileList, params, nb_params);
}

ParameterizedTest(const struct TestFileList *tup, collection_comparator, test_collection_changes)
{
  TestData *data = test_data_new();
  CollectionComparator *comporator = collection_comparator_new();
  collection_comporator_set_callbacks(comporator,
                                      _handle_new,
                                      _handle_delete,
                                      data);
  gint ndx = tup->ndx;
  const CollectionComparatorEntry *curr_entry = NULL;
  gint i;

  cr_log_info("=== %d. %s", ndx, tup->name);

  for (i = 0; curr_entry = &tup->initial_files[i], curr_entry->value != NULL; i++)
    collection_comparator_add_initial_value(comporator, tup->initial_files[i].key, tup->initial_files[i].value);

  collection_comparator_start(comporator);
  for (i = 0; curr_entry = &tup->result_files[i], curr_entry->value != NULL; i++)
    collection_comparator_add_value(comporator, curr_entry->key, curr_entry->value);
  collection_comparator_stop(comporator);

  cr_assert(ndx == 0 || ndx == 4 ?
            g_hash_table_size(data->deleted_entries) == 0 :
            g_hash_table_size(data->deleted_entries) > 0);
  for (i = 0; curr_entry = &tup->expected_deleted_files[i], curr_entry->value != NULL; i++)
    {
      CollectionComparatorEntry *found = g_hash_table_lookup(data->deleted_entries, curr_entry);
      cr_assert(found);
      cr_assert_str_eq(found->value, curr_entry->value);
      cr_assert_eq(memcmp(found->key, curr_entry->key, 2 * sizeof(gint64)), 0);
    }
  cr_assert_eq(g_hash_table_size(data->deleted_entries), i);

  cr_assert(ndx != 4 && ndx != 5 ?
            g_hash_table_size(data->new_entries) == 0 :
            g_hash_table_size(data->new_entries) > 0);
  for (i = 0; curr_entry = &tup->expected_new_files[i], curr_entry->value != NULL; i++)
    {
      CollectionComparatorEntry *found = g_hash_table_lookup(data->new_entries, curr_entry);
      cr_assert(found);
      cr_assert_str_eq(found->value, curr_entry->value);
      cr_assert_eq(memcmp(found->key, curr_entry->key, 2 * sizeof(gint64)), 0);
    }
  cr_assert_eq(g_hash_table_size(data->new_entries), i);

  collection_comparator_free(comporator);
  test_data_free(data);
}
