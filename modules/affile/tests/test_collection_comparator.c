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
#include <glib/gstdio.h>
#include <unistd.h>
#include "collection-comparator.h"

typedef struct _TestData
{
  GHashTable *deleted_entries;
  GHashTable *new_entries;
} TestData;

static const gchar *TEST = "test";

TestData *
test_data_new(void)
{
  TestData *self = g_new0(TestData, 1);
  self->deleted_entries = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  self->new_entries = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  return self;
}

void test_data_free(TestData *self)
{
  g_hash_table_unref(self->deleted_entries);
  g_hash_table_unref(self->new_entries);
  g_free(self);
}

static void
_handle_new(const gchar *filename, gpointer data)
{
  TestData *self = (TestData *)data;
  g_hash_table_insert(self->new_entries, g_strdup(filename), (gchar *)TEST);
}

static void
_handle_delete(const gchar *filename, gpointer data)
{
  TestData *self = (TestData *)data;
  g_hash_table_insert(self->deleted_entries, g_strdup(filename), (gchar *)TEST);
}

struct TestFileList
{
  const gchar *initial_files[10];
  const gchar *result_files[10];
  const gchar *expected_deleted_files[10];
  const gchar *expected_new_files[10];
};

struct TestFileList nothing_changed = {{"test1", "test2"}, {"test1", "test2"}, {NULL}, {NULL}};
struct TestFileList last_file_deleted = {{"test1", "test2"}, {"test2"}, {"test1"}, {NULL}};
struct TestFileList first_file_deleted = {{"test1", "test2"}, {"test1"}, {"test2"}, {NULL}};
struct TestFileList delete_all = {{"test1", "test2"}, {NULL}, {"test1", "test2"}, {NULL}};
struct TestFileList create_all = {{NULL}, {"test1", "test2"}, {NULL}, {"test1", "test2"}};
struct TestFileList mixed = {{"test1", "test2", "test3"}, {"test1", "test4"}, {"test2", "test3"}, {"test4"}};

ParameterizedTestParameters(params, multiple)
{
  static struct TestFileList params[6];
  params[0] = nothing_changed;
  params[1] = last_file_deleted;
  params[2] = first_file_deleted;
  params[3] = delete_all;
  params[4] = create_all;
  params[5] = mixed;

  return cr_make_param_array(struct TestFileList, params, sizeof (params) / sizeof (struct TestFileList));
}

ParameterizedTest(struct TestFileList *tup, params, multiple)
{
  TestData *data = test_data_new();
  CollectionComparator *comporator = collection_comparator_new();
  collection_comporator_set_callbacks(comporator,
                                      _handle_new,
                                      _handle_delete,
                                      data);
  gint i;
  for (i = 0; tup->initial_files[i] != NULL; i++)
    {
      collection_comparator_add_initial_value(comporator, tup->initial_files[i]);
    }

  collection_comparator_start(comporator);

  for (i = 0; tup->result_files[i] != NULL; i++)
    {
      collection_comparator_add_value(comporator, tup->result_files[i]);
    }
  collection_comparator_stop(comporator);

  for (i = 0; tup->expected_deleted_files[i] != NULL; i++)
    {
      cr_assert_str_eq(g_hash_table_lookup(data->deleted_entries, tup->expected_deleted_files[i]), TEST);
    }
  cr_assert_eq(g_hash_table_size(data->deleted_entries), i);

  for (i = 0; tup->expected_new_files[i] != NULL; i++)
    {
      cr_assert_str_eq(g_hash_table_lookup(data->new_entries, tup->expected_new_files[i]), TEST);
    }
  cr_assert_eq(g_hash_table_size(data->new_entries), i);

  collection_comparator_free(comporator);
  test_data_free(data);
}
