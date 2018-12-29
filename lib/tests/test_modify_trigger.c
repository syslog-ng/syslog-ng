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

#include <criterion/criterion.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include "collection-comparator.h"

typedef struct _TestData
{
  GHashTable *deleted_entries;
  GHashTable *new_entries;
  GHashTable *modified_entries;
} TestData;

static const gchar *TEST = "test";

TestData *
test_data_new(void)
{
  TestData *self = g_new0(TestData, 1);
  self->deleted_entries = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  self->new_entries = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  self->modified_entries = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  return self;
}

void test_data_free(TestData *self)
{
  g_hash_table_unref(self->deleted_entries);
  g_hash_table_unref(self->new_entries);
  g_hash_table_unref(self->modified_entries);
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

static void
_handle_modify(const gchar *filename, gpointer data)
{
  TestData *self = (TestData *)data;
  g_hash_table_insert(self->modified_entries, g_strdup(filename), (gchar *)TEST);
}

Test(collection_comparator, modified_entry)
{
  TestData *data = test_data_new();
  CollectionComparator *comporator = collection_comparator_new();
  collection_comporator_set_callbacks(comporator,
                                      _handle_new,
                                      _handle_delete,
                                      _handle_modify,
                                      data);

  collection_comparator_add_initial_value(comporator, "foo", 1);
  collection_comparator_add_initial_value(comporator, "bar", 2);

  collection_comparator_start(comporator);

  collection_comparator_add_value(comporator, "foo", 2);
  collection_comparator_add_value(comporator, "bar", 2);

  collection_comparator_stop(comporator);

  cr_assert_eq(g_hash_table_size(data->deleted_entries), 0);
  cr_assert_eq(g_hash_table_size(data->new_entries), 0);
  cr_assert_eq(g_hash_table_size(data->modified_entries), 1);

  cr_assert_str_eq(g_hash_table_lookup(data->modified_entries, "foo"), TEST);

  collection_comparator_free(comporator);
  test_data_free(data);
}
