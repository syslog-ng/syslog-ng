/*
 * Copyright (c) 2016 Balabit
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

#include "kvtagdb.h"
#include "testutils.h"
#include <stdio.h>
#include <string.h>

#define KVTAGDB_TESTCASE(testfunc, ...) {testcase_begin("%s(%s)", #testfunc, #__VA_ARGS__); testfunc(__VA_ARGS__); testcase_end();}

static void
_foreach_ctr(gpointer arg, const TagRecord *record)
{
  int *ctr = (int *)arg;
  ++(*ctr);
}

static void
_test_empty_db(KVTagDB *tagdb)
{
  assert_false(kvtagdb_is_loaded(tagdb) == TRUE, "Empty KVTagDB should be in unloaded state.");
  assert_false(kvtagdb_is_indexed(tagdb) == TRUE, "Empty KVTagDB should be in un-indexed state.");
  assert_false(kvtagdb_contains(tagdb, "selector") == TRUE, "Method kvtagdb_contains should work with empty KVTagDB.");
  assert_true(kvtagdb_number_of_tags(tagdb, "selector") == 0, "Method kvtagdb_number should work with empty KVTagDB.");
  int ctr = 0;
  kvtagdb_foreach_tag(tagdb, "selector", _foreach_ctr, (gpointer)&ctr);
  assert_true(ctr == 0, "Method kvtagdb_foreach_tag should work for with empty KVTagDB.");
}

static void
test_empty_db()
{
  KVTagDB *tagdb = kvtagdb_new();

  _test_empty_db(tagdb);

  kvtagdb_unref(tagdb);
}

static void
test_purge_empty_db()
{
  KVTagDB *tagdb = kvtagdb_new();

  kvtagdb_purge(tagdb);
  _test_empty_db(tagdb);

  kvtagdb_unref(tagdb);
}

static void
test_index_empty_db()
{
  KVTagDB *tagdb = kvtagdb_new();

  kvtagdb_index(tagdb);
  _test_empty_db(tagdb);

  kvtagdb_unref(tagdb);
}

static void
_fill_tagdb(KVTagDB *tagdb, const gchar *selector_base,
            const gchar *name_base, const gchar *value_base,
            int number_of_selectors, int number_of_nv_pairs_per_selector)
{
  int i, j;
  for (i = 0; i < number_of_selectors; i++)
    {
      for (j = 0; j < number_of_nv_pairs_per_selector; j++)
        {
          TagRecord record = {
            .selector = g_strdup_printf("%s-%d", selector_base, i),
            .name     = g_strdup_printf("%s-%d.%d", name_base, i, j),
            .value    = g_strdup_printf("%s-%d.%d", value_base, i, j)
          };
          kvtagdb_insert(tagdb, &record);
        }
    }
}

static gint
_g_strcmp(const gconstpointer a, gconstpointer b)
{
  return g_strcmp0((const gchar *)a, (const gchar *)b);
}

static void
test_insert()
{
  KVTagDB *tagdb = kvtagdb_new();

  _fill_tagdb(tagdb, "selector", "name", "value", 2, 5);
  int ctr = 0;
  assert_true(kvtagdb_number_of_tags(tagdb, "selector-0") == 5, "selector-0 should have 5 nv-pairs");
  kvtagdb_foreach_tag(tagdb, "selector-0", _foreach_ctr, (gpointer)&ctr);  
  assert_true(ctr == 5, "foreach should find 5 nv-pairs for selector-0");
  
  kvtagdb_unref(tagdb);
}

static void
test_get_selectors()
{
  KVTagDB *tagdb = kvtagdb_new();

  _fill_tagdb(tagdb, "selector", "name", "value", 2, 5);

  GList *selectors = kvtagdb_get_selectors(tagdb);
  GList *selector0 = g_list_find_custom(selectors, "selector-0", _g_strcmp);
  GList *selector1 = g_list_find_custom(selectors, "selector-1", _g_strcmp);

  assert_string((gchar *)selector0->data, "selector-0", "");
  assert_string((gchar *)selector1->data, "selector-1", "");
  
  g_list_free(selectors);

  kvtagdb_unref(tagdb);
}

typedef struct _TestNVPair
{
  const gchar *name;
  const gchar *value;
} TestNVPair;

typedef struct _TestNVPairStore
{
  TestNVPair *pairs;
  int ctr;
} TestNVPairStore;

static void
_foreach_get_nvpairs(gpointer arg, const TagRecord *record)
{
  TestNVPairStore *store = (TestNVPairStore *)arg;
  TestNVPair pair = {.name=record->name, .value=record->value};
  store->pairs[store->ctr++] = pair;
}

static void
_assert_kvtag_db_contains_name_value_pairs_by_selector(KVTagDB *tagdb, 
                                                       const gchar *selector,
                                                       TestNVPair *expected_nvpairs,
                                                       guint number_of_expected_nvpairs)
{
  TestNVPair result[number_of_expected_nvpairs];
  TestNVPairStore result_store = {.pairs = result, .ctr = 0};
  
  kvtagdb_foreach_tag(tagdb, selector, _foreach_get_nvpairs, (gpointer) &result_store);
  assert_true(result_store.ctr == number_of_expected_nvpairs, "");
  guint i;
  for (i = 0; i < number_of_expected_nvpairs; i++)
    {
      assert_string(result[i].name, expected_nvpairs[i].name, "");
      assert_string(result[i].value, expected_nvpairs[i].value, "");
    }
}

static void
test_inserted_nv_pairs()
{
  KVTagDB *tagdb = kvtagdb_new();
  _fill_tagdb(tagdb, "selector", "name", "value", 1, 3);

 TestNVPair expected_nvpairs[3] = {
    {.name="name-0.0", .value="value-0.0"},
    {.name="name-0.1", .value="value-0.1"},
    {.name="name-0.2", .value="value-0.2"}
  };

  _assert_kvtag_db_contains_name_value_pairs_by_selector(tagdb, "selector-0", expected_nvpairs, 3);
  kvtagdb_unref(tagdb);
}

static void
test_import_with_valid_csv()
{
  gchar csv_content[] = "selector1,name1,value1\n"\
                        "selector1,name1.1,value1.1\n"\
                        "selector2,name2,value2\n"\
                        "selector3,name3,value3";
  FILE *fp = fmemopen(csv_content, strlen(csv_content) + 1, "r");
  KVTagDB *db = kvtagdb_new();
  TagRecordScanner *scanner = create_tag_record_scanner_by_type("csv");

  assert_true(kvtagdb_import(db, fp, scanner), "Failed to import valid CSV file.");
  assert_true(kvtagdb_is_loaded(db), "The kvtag_db_is_loaded reports False after a successful import operation. ");
  assert_true(kvtagdb_is_indexed(db), "The kvtag_db_is_indexed reports False after successful import&load operations.");

  TestNVPair expected_nvpairs_selector1[2] = {
    {.name="name1", .value="value1"},
    {.name="name1.1", .value="value1.1"},
  };

  TestNVPair expected_nvpairs_selector2[1] = {
    {.name="name2", .value="value2"},
  };

  TestNVPair expected_nvpairs_selector3[1] = {
    {.name="name3", .value="value3"},
  };

  _assert_kvtag_db_contains_name_value_pairs_by_selector(db, "selector1", expected_nvpairs_selector1, 2);
  _assert_kvtag_db_contains_name_value_pairs_by_selector(db, "selector2", expected_nvpairs_selector2, 1);
  _assert_kvtag_db_contains_name_value_pairs_by_selector(db, "selector3", expected_nvpairs_selector3, 1);

  kvtagdb_free(db);
  tag_record_scanner_free(scanner);
}

static void
test_import_with_invalid_csv_content()
{
  gchar csv_content[] = "xxx";
  FILE *fp = fmemopen(csv_content, strlen(csv_content) + 1, "r");
  KVTagDB *db = kvtagdb_new();
  TagRecordScanner *scanner = create_tag_record_scanner_by_type("csv");

  assert_false(kvtagdb_import(db, fp, scanner), "Sucessfully import an invalid CSV file.");
  assert_false(kvtagdb_is_loaded(db), "The kvtag_db_is_loaded reports True after a failing import operation. ");
  assert_false(kvtagdb_is_indexed(db), "The kvtag_db_is_indexed reports True after failing import&load operations.");

  kvtagdb_free(db);
  tag_record_scanner_free(scanner);
}

int main(int argc, char **argv)
{
  KVTAGDB_TESTCASE(test_empty_db);
  KVTAGDB_TESTCASE(test_purge_empty_db);
  KVTAGDB_TESTCASE(test_index_empty_db);
  KVTAGDB_TESTCASE(test_insert);
  KVTAGDB_TESTCASE(test_get_selectors);
  KVTAGDB_TESTCASE(test_inserted_nv_pairs);
  KVTAGDB_TESTCASE(test_import_with_valid_csv);
  KVTAGDB_TESTCASE(test_import_with_invalid_csv_content);

  return 0;
}
