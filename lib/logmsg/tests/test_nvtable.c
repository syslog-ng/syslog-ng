/*
 * Copyright (c) 2009-2016 Balabit
 * Copyright (c) 2009-2016 Balázs Scheidler
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

#include "logmsg/nvtable.h"
#include "apphook.h"
#include "logmsg/logmsg.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define STATIC_VALUES 16
#define STATIC_HANDLE 1
#define STATIC_NAME   "VAL1"
#define DYN_HANDLE 17
#define DYN_NAME "VAL17"

void
assert_nvtable(NVTable *tab, NVHandle handle, gchar *expected_value, gssize expected_length)
{
  const gchar *value;
  gssize length;

  value = nv_table_get_value(tab, handle, &length);

  cr_assert_eq(length, expected_length,
               "NVTable value mismatch, value=%.*s, expected=%.*s\n",
               (gint) length, value, (gint) expected_length, expected_value);

  cr_assert_arr_eq(value, expected_value, expected_length,
                   "NVTable value mismatch, value=%.*s, expected=%.*s\n",
                   (gint) length, value, (gint) expected_length, expected_value);
}


TestSuite(nvtable, .init = app_startup, .fini = app_shutdown);

/* NVRegistry */
/* testcases:
 *
 *   - static names will have the lowest numbered handles, starting with 1
 *   - registering the same name will always map to the same handle
 *   - adding an alias and looking up an NV pair by alias should return the same handle
 *   - NV pairs cannot have the zero-length string as a name
 *   - NV pairs cannot have names longer than 255 characters
 *   - no more than predefined number of NV pairs are supported
 *   -
 */

#define TEST_NVHANDLE_MAX_VALUE 10
Test(nvtable, test_nv_registry)
{
  NVRegistry *reg;
  gint i, j;
  NVHandle handle, prev_handle;
  const gchar *name;
  gssize len;
  const gchar *builtins[] = { "BUILTIN1", "BUILTIN2", "BUILTIN3", NULL };

  reg = nv_registry_new(builtins, TEST_NVHANDLE_MAX_VALUE);

  for (i = 0; builtins[i]; i++)
    {
      handle = nv_registry_alloc_handle(reg, builtins[i]);
      cr_assert_eq(handle, (i+1));
      name = nv_registry_get_handle_name(reg, handle, &len);
      cr_assert_str_eq(name, builtins[i]);
      cr_assert_eq(strlen(name), len);
    }

  for (i = 4; i < TEST_NVHANDLE_MAX_VALUE + 1; i++)
    {
      gchar dyn_name[16];

      g_snprintf(dyn_name, sizeof(dyn_name), "DYN%05d", i);

      /* try to look up the same name multiple times */
      prev_handle = 0;
      for (j = 0; j < 4; j++)
        {
          handle = nv_registry_alloc_handle(reg, dyn_name);
          g_assert(prev_handle == 0 || (handle == prev_handle));
          prev_handle = handle;
        }
      name = nv_registry_get_handle_name(reg, handle, &len);
      g_assert(strcmp(name, dyn_name) == 0);
      g_assert(strlen(name) == len);

      g_snprintf(dyn_name, sizeof(dyn_name), "ALIAS%05d", i);
      nv_registry_add_alias(reg, handle, dyn_name);
      handle = nv_registry_alloc_handle(reg, dyn_name);
      g_assert(handle == prev_handle);
    }

  for (i = TEST_NVHANDLE_MAX_VALUE; i >= 4; i--)
    {
      gchar dyn_name[16];

      g_snprintf(dyn_name, sizeof(dyn_name), "DYN%05d", i);

      /* try to look up the same name multiple times */
      prev_handle = 0;
      for (j = 0; j < 4; j++)
        {
          handle = nv_registry_alloc_handle(reg, dyn_name);
          g_assert(prev_handle == 0 || (handle == prev_handle));
          prev_handle = handle;
        }
      name = nv_registry_get_handle_name(reg, handle, &len);
      g_assert(strcmp(name, dyn_name) == 0);
      g_assert(strlen(name) == len);
    }

  handle = nv_registry_alloc_handle(reg, "too-many-values");
  cr_assert_eq(handle, 0);

  nv_registry_free(reg);
}

/*
 *  - NVTable direct values
 *    - set/get static NV entries
 *      - new NV entry
 *        - entries that fit into the current NVTable
 *        - entries that do not fit into the current NVTable
 *      - overwrite direct NV entry
 *        - value that fits into the current entry
 *        - value that doesn't fit into the current entry, but fits into NVTable
 *        - value that doesn't fit into the current entry and neither to NVTable
 *      - overwrite indirect NV entry: not possible as static entries cannot be indirect
 *
 *    - set/get dynamic NV entries
 *      - new NV entry
 *        - entries that fit into the current NVTable
 *        - entries that do not fit into the current NVTable
 *      - overwrite direct NV entry
 *        - value that fits into the current entry
 *        - value that doesn't fit into the current entry, but fits into NVTable
 *        - value that doesn't fit into the current entry and neither to NVTable
 *      - overwrite indirect NV entry
 *        - value that fits into the current entry
 *        - value that doesn't fit into the current entry, but fits into NVTable
 *        - value that doesn't fit into the current entry and neither to NVTable
 */

Test(nvtable, test_nvtable_direct)
{
  NVTable *tab;
  NVHandle handle;
  gchar value[1024], name[16];
  gboolean success;
  gint i;
  guint16 used;

  for (i = 0; i < sizeof(value); i++)
    value[i] = 'A' + (i % 26);

  handle = STATIC_HANDLE;
  do
    {
      g_snprintf(name, sizeof(name), "VAL%d", handle);

      /*************************************************************/
      /* new NV entry */
      /*************************************************************/

      /* one that fits */
      tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
      success = nv_table_add_value(tab, handle, name, strlen(name), value, 128, NULL);
      cr_assert(success);
      assert_nvtable(tab, handle, value, 128);
      nv_table_unref(tab);

      /* one that is too large */
      tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
      success = nv_table_add_value(tab, handle, name, strlen(name), value, 512, NULL);
      cr_assert_not(success);
      nv_table_unref(tab);

      /*************************************************************/
      /* overwrite NV entry */
      /*************************************************************/

      /* one that fits, but realloced size wouldn't fit */
      tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
      success = nv_table_add_value(tab, handle, name, strlen(name), value, 128, NULL);
      cr_assert(success);
      used = tab->used;

      success = nv_table_add_value(tab, handle, name, strlen(name), value, 64, NULL);
      cr_assert(success);
      cr_assert_eq(tab->used, used);
      assert_nvtable(tab, handle, value, 64);
      nv_table_unref(tab);

      /* one that is too large for the given entry, but still fits in the NVTable */
      tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
      success = nv_table_add_value(tab, handle, name, strlen(name), value, 64, NULL);
      cr_assert(success);
      used = tab->used;

      success = nv_table_add_value(tab, handle, name, strlen(name), value, 128, NULL);
      cr_assert(success);
      cr_assert_gt(tab->used, used);
      assert_nvtable(tab, handle, value, 128);
      nv_table_unref(tab);

      /* one that is too large for the given entry, and also for the NVTable */
      tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
      success = nv_table_add_value(tab, handle, name, strlen(name), value, 64, NULL);
      cr_assert(success);

      success = nv_table_add_value(tab, handle, name, strlen(name), value, 512, NULL);
      cr_assert_not(success);
      assert_nvtable(tab, handle, value, 64);
      nv_table_unref(tab);

      /*************************************************************/
      /* overwrite indirect entry */
      /*************************************************************/

      if (handle > STATIC_VALUES)
        {
          /* we can only test this with dynamic entries */

          /* setup code: add static and a dynamic-indirect entry */
          tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
          success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
          cr_assert(success);
          success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                                &(NVReferencedSlice)
          {
            STATIC_HANDLE, 1, 126, 0
          }, NULL);
          cr_assert(success);
          used = tab->used;

          /* store a direct entry over the indirect one */
          success = nv_table_add_value(tab, handle, name, strlen(name), value, 1, NULL);
          cr_assert(success);
          cr_assert_eq(tab->used, used);
          assert_nvtable(tab, STATIC_HANDLE, value, 128);
          assert_nvtable(tab, handle, value, 1);

          nv_table_unref(tab);

          /* setup code: add static and a dynamic-indirect entry */
          tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
          success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 64, NULL);
          cr_assert(success);
          success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                                &(NVReferencedSlice)
          {
            STATIC_HANDLE, 1, 63, 0
          }, NULL);
          cr_assert(success);

          used = tab->used;

          /* store a direct entry over the indirect one, we don't fit in the allocated space */
          success = nv_table_add_value(tab, handle, name, strlen(name), value, 128, NULL);
          cr_assert(success);
          cr_assert_gt(tab->used, used);
          assert_nvtable(tab, STATIC_HANDLE, value, 64);
          assert_nvtable(tab, handle, value, 128);

          nv_table_unref(tab);

          /* setup code: add static and a dynamic-indirect entry */
          tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
          success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 64, NULL);
          cr_assert(success);
          success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                                &(NVReferencedSlice)
          {
            STATIC_HANDLE, 1, 62, 0
          }, NULL);
          cr_assert(success);
          used = tab->used;

          /* store a direct entry over the indirect one, we don't fit in the allocated space */
          success = nv_table_add_value(tab, handle, name, strlen(name), value, 256, NULL);
          cr_assert_not(success);
          assert_nvtable(tab, STATIC_HANDLE, value, 64);
          assert_nvtable(tab, handle, value + 1, 62);

          nv_table_unref(tab);

        }

      handle += STATIC_VALUES;
    }
  while (handle < 2 * STATIC_VALUES);
}

/*
 *  - indirect values
 *    - indirect static values are not possible
 *    - set/get dynamic NV entries that refer to direct entries
 *      - new NV entry
 *        - entries that fit into the current NVTable
 *        - entries that do not fit into the current NVTable
 *      - overwrite indirect NV entry
 *        - value always fits to a current NV entry, no other cases
 *      - overwrite direct NV entry
 *        - value that fits into the current entry
 *        - value that doesn't fit into the current entry, but fits into NVTable
 *        - value that doesn't fit into the current entry and neither to NVTable
 *    - set/get dynamic NV entries that refer to indirect entry, they become direct entries
 *      - new NV entry
 *        - entries that fit into the current NVTable
 *        - entries that do not fit into the current NVTable
 *      - overwrite indirect NV entry
 *        - value that fits into the current entry
 *        - value that doesn't fit into the current entry, but fits into NVTable
 *        - value that doesn't fit into the current entry and neither to NVTable
 *      - overwrite direct NV entry
 *        - value that fits into the current entry
 *        - value that doesn't fit into the current entry, but fits into NVTable
 *        - value that doesn't fit into the current entry and neither to NVTable
 *    - set/get dynamic NV entries that refer to a non-existent entry
 *        -
 */
Test(nvtable, test_nvtable_indirect)
{
  NVTable *tab;
  NVHandle handle;
  gchar value[1024], name[16];
  gboolean success;
  gint i;
  guint16 used;

  for (i = 0; i < sizeof(value); i++)
    value[i] = 'A' + (i % 26);

  handle = DYN_HANDLE+1;
  g_snprintf(name, sizeof(name), "VAL%d", handle);
  fprintf(stderr, "Testing indirect values, name: %s, handle: %d\n", name, handle);

  /*************************************************************/
  /*************************************************************/
  /* indirect entries that refer to direct entries */
  /*************************************************************/
  /*************************************************************/

  /*************************************************************/
  /* new NV entry */
  /*************************************************************/

  /* one that fits */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, handle, value + 1, 126);
  nv_table_unref(tab);

  /* one that is too large */

  /* NOTE: the sizing of the NVTable can easily be broken, it is sized
     to make it possible to store one direct entry */

  tab = nv_table_new(STATIC_VALUES, 0, 138+3);  // direct: +3
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert_not(success);

  nv_table_unref(tab);

  /*************************************************************/
  /* overwrite NV entry */
  /*************************************************************/

  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);
  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 62, 0
  }, NULL);

  cr_assert(success);
  cr_assert_eq(used, tab->used);
  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, handle, value + 1, 62);
  nv_table_unref(tab);


  /*************************************************************/
  /* overwrite direct entry */
  /*************************************************************/

  /* the new entry fits to the space allocated to the old */

  /* setup code: add static and a dynamic-direct entry */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);
  success = nv_table_add_value(tab, handle, name, strlen(name), value, 64, NULL);
  cr_assert(success);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  cr_assert_eq(tab->used, used);
  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, handle, value + 1, 126);

  nv_table_unref(tab);

  /* the new entry will not fit to the space allocated to the old */

  /* setup code: add static and a dynamic-direct entry */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);
  success = nv_table_add_value(tab, handle, name, strlen(name), value, 1, NULL);
  cr_assert(success);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  cr_assert_gt(tab->used, used);
  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, handle, value + 1, 126);

  nv_table_unref(tab);

  /* the new entry will not fit to the space allocated to the old and neither to the NVTable */

  /* setup code: add static and a dynamic-direct entry */
  tab = nv_table_new(STATIC_VALUES, 1, 154+3+4);  // direct: +3, indirect: +4
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);
  success = nv_table_add_value(tab, handle, name, strlen(name), value, 1, NULL);
  cr_assert(success);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert_not(success);
  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, handle, value, 1);

  nv_table_unref(tab);

  /*************************************************************/
  /*************************************************************/
  /* indirect entries that refer to indirect entries */
  /*************************************************************/
  /*************************************************************/


  /*************************************************************/
  /* new NV entry */
  /*************************************************************/

  /* one that fits */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    DYN_HANDLE, 1, 122, 0
  }, NULL);
  cr_assert(success);
  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, DYN_HANDLE, value + 1, 126);
  assert_nvtable(tab, handle, value + 2, 122);
  nv_table_unref(tab);

  /* one that is too large */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    DYN_HANDLE, 1, 122, 0
  }, NULL);
  cr_assert_not(success);
  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, DYN_HANDLE, value + 1, 126);
  assert_nvtable(tab, handle, "", 0);
  nv_table_unref(tab);

  /*************************************************************/
  /* overwrite indirect NV entry */
  /*************************************************************/


  /* we fit to the space of the old */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    DYN_HANDLE, 1, 1, 0
  }, NULL);
  cr_assert(success);

  cr_assert_eq(tab->used, used);
  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, DYN_HANDLE, value + 1, 126);
  assert_nvtable(tab, handle, value + 2, 1);
  nv_table_unref(tab);

  /* the new entry will not fit to the space allocated to the old */

  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    DYN_HANDLE, 1, 16, 0
  }, NULL);
  cr_assert(success);

  cr_assert_gt(tab->used, used);
  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, DYN_HANDLE, value + 1, 126);
  assert_nvtable(tab, handle, value + 2, 16);
  nv_table_unref(tab);

  /* one that is too large */

  tab = nv_table_new(STATIC_VALUES, 4, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    DYN_HANDLE, 1, 124, 0
  }, NULL);
  cr_assert_not(success);

  cr_assert_eq(tab->used, used);
  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, DYN_HANDLE, value + 1, 126);
  assert_nvtable(tab, handle, value + 1, 126);
  nv_table_unref(tab);

  /*************************************************************/
  /* overwrite direct NV entry */
  /*************************************************************/


  /* we fit to the space of the old */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  success = nv_table_add_value(tab, handle, name, strlen(name), value, 64, NULL);
  cr_assert(success);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    DYN_HANDLE, 1, 16, 0
  }, NULL);
  cr_assert(success);

  cr_assert_eq(tab->used, used);
  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, DYN_HANDLE, value + 1, 126);
  assert_nvtable(tab, handle, value + 2, 16);
  nv_table_unref(tab);


  /* the new entry will not fit to the space allocated to the old */

  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  success = nv_table_add_value(tab, handle, name, strlen(name), value, 16, NULL);
  cr_assert(success);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    DYN_HANDLE, 1, 32, 0
  }, NULL);
  cr_assert(success);

  cr_assert_gt(tab->used, used);
  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, DYN_HANDLE, value + 1, 126);
  assert_nvtable(tab, handle, value + 2, 32);
  nv_table_unref(tab);

  /* one that is too large */

  tab = nv_table_new(STATIC_VALUES, 4, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  success = nv_table_add_value(tab, handle, name, strlen(name), value, 16, NULL);
  cr_assert(success);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    DYN_HANDLE, 1, 124, 0
  }, NULL);
  cr_assert_not(success);

  cr_assert_eq(tab->used, used);
  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, DYN_HANDLE, value + 1, 126);
  assert_nvtable(tab, handle, value, 16);
  nv_table_unref(tab);

  /*************************************************************/
  /* indirect that refers to non-existent entry */
  /*************************************************************/
  /* one that fits */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  cr_assert_eq(used, tab->used);
  assert_nvtable(tab, STATIC_HANDLE, "", 0);
  assert_nvtable(tab, handle, "", 0);
  nv_table_unref(tab);
}

/*
 *
 *  - other corner cases
 *    - set zero length value in direct value
 *      - add a zero-length value to non-existing entry
 *      - add a zero-length value to an existing entry that is not zero-length
 *      - add a zero-length value to an existing entry that is zero-length
 *    - set zero length value in an indirect value
 *
 * - change an entry that is referenced by other entries
 */
Test(nvtable, test_nvtable_others)
{
  NVTable *tab;
  NVHandle handle;
  gchar value[1024], name[16];
  gboolean success;
  gint i;

  for (i = 0; i < sizeof(value); i++)
    value[i] = 'A' + (i % 26);

  handle = DYN_HANDLE+1;
  g_snprintf(name, sizeof(name), "VAL%d", handle);
  fprintf(stderr, "Testing other cases, name: %s, handle: %d\n", name, handle);

  /* one that fits */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value + 32, 32, NULL);
  cr_assert(success);

  assert_nvtable(tab, STATIC_HANDLE, value + 32, 32);
  assert_nvtable(tab, handle, value + 1, 126);
  nv_table_unref(tab);

  /* one that doesn't fit */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  cr_assert(success);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name),
                                        &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 126, 0
  }, NULL);
  cr_assert(success);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value + 32, 32, NULL);
  cr_assert_not(success);

  assert_nvtable(tab, STATIC_HANDLE, value, 128);
  assert_nvtable(tab, handle, value + 1, 126);
  nv_table_unref(tab);
}

Test(nvtable, test_nvtable_lookup)
{
  NVTable *tab;
  NVHandle handle;
  gchar name[16];
  gboolean success;
  gint i;
  NVHandle handles[100];
  gint x;

  srand(time(NULL));
  for (x = 0; x < 100; x++)
    {
      /* test dynamic lookup */
      tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 4096);

      for (i = 0; i < 100; i++)
        {
          do
            {
              handle = rand() & 0x8FFF;
            }
          while (handle == 0);
          g_snprintf(name, sizeof(name), "VAL%d", handle);
          success = nv_table_add_value(tab, handle, name, strlen(name), name, strlen(name), NULL);
          g_assert(success);
          handles[i] = handle;
        }

      for (i = 99; i >= 0; i--)
        {
          handle = handles[i];
          g_snprintf(name, sizeof(name), "VAL%d", handle);
          assert_nvtable(tab, handles[i], name, strlen(name));
          g_assert(nv_table_is_value_set(tab, handles[i]));

        }
      cr_assert_not(nv_table_is_value_set(tab, 0xFE00));
      nv_table_unref(tab);
    }
}

Test(nvtable, test_nvtable_clone_grows_the_cloned_structure)
{
  NVTable *tab, *tab_clone;
  gboolean success;

  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 64);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, strlen(STATIC_NAME), "value", 5, NULL);
  cr_assert(success);
  assert_nvtable(tab, STATIC_HANDLE, "value", 5);

  tab_clone = nv_table_clone(tab, 64);
  assert_nvtable(tab_clone, STATIC_HANDLE, "value", 5);
  cr_assert_lt(tab->size, tab_clone->size);
  nv_table_unref(tab_clone);
  nv_table_unref(tab);
}

Test(nvtable, test_nvtable_clone_cannot_grow_nvtable_larger_than_nvtable_max_bytes)
{
  NVTable *tab, *tab_clone;
  gboolean success;

  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, NV_TABLE_MAX_BYTES - 1024);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, strlen(STATIC_NAME), "value", 5, NULL);
  cr_assert(success);
  assert_nvtable(tab, STATIC_HANDLE, "value", 5);

  tab_clone = nv_table_clone(tab, 2048);
  assert_nvtable(tab_clone, STATIC_HANDLE, "value", 5);
  cr_assert_lt(tab->size, tab_clone->size);
  cr_assert_leq(tab_clone->size, NV_TABLE_MAX_BYTES);
  nv_table_unref(tab_clone);
  nv_table_unref(tab);
}

Test(nvtable, test_nvtable_realloc_doubles_nvtable_size)
{
  NVTable *tab;
  gboolean success;
  gsize old_size;

  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 1024);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, strlen(STATIC_NAME), "value", 5, NULL);
  cr_assert(success);
  assert_nvtable(tab, STATIC_HANDLE, "value", 5);

  old_size = tab->size;

  cr_assert(nv_table_realloc(tab, &tab));
  cr_assert_geq(tab->size, old_size * 2);
  assert_nvtable(tab, STATIC_HANDLE, "value", 5);

  nv_table_unref(tab);

}

Test(nvtable, test_nvtable_realloc_sets_size_to_nv_table_max_bytes_at_most)
{
  NVTable *tab;
  gboolean success;
  gsize old_size;

  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, NV_TABLE_MAX_BYTES - 1024);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, strlen(STATIC_NAME), "value", 5, NULL);
  cr_assert(success);
  assert_nvtable(tab, STATIC_HANDLE, "value", 5);

  old_size = tab->size;

  cr_assert(nv_table_realloc(tab, &tab));
  cr_assert_gt(tab->size, old_size);
  cr_assert_leq(tab->size, NV_TABLE_MAX_BYTES);
  assert_nvtable(tab, STATIC_HANDLE, "value", 5);

  nv_table_unref(tab);
}

Test(nvtable, test_nvtable_realloc_fails_if_size_is_at_maximum)
{
  NVTable *tab;
  gboolean success;

  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, NV_TABLE_MAX_BYTES);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, strlen(STATIC_NAME), "value", 5, NULL);
  cr_assert(success);
  assert_nvtable(tab, STATIC_HANDLE, "value", 5);

  cr_assert_not(nv_table_realloc(tab, &tab));
  cr_assert_eq(tab->size, NV_TABLE_MAX_BYTES);
  assert_nvtable(tab, STATIC_HANDLE, "value", 5);

  nv_table_unref(tab);
}

Test(nvtable, test_nvtable_realloc_leaves_original_intact_if_there_are_multiple_references)
{
  NVTable *tab_ref1, *tab_ref2;
  gboolean success;
  gsize old_size;

  tab_ref1 = nv_table_new(STATIC_VALUES, STATIC_VALUES, 1024);
  tab_ref2 = nv_table_ref(tab_ref1);

  success = nv_table_add_value(tab_ref1, STATIC_HANDLE, STATIC_NAME, strlen(STATIC_NAME), "value", 5, NULL);
  cr_assert(success);
  assert_nvtable(tab_ref1, STATIC_HANDLE, "value", 5);
  assert_nvtable(tab_ref2, STATIC_HANDLE, "value", 5);

  old_size = tab_ref1->size;

  cr_assert(nv_table_realloc(tab_ref2, &tab_ref2));
  cr_assert_eq(tab_ref1->size, old_size);
  cr_assert_geq(tab_ref2->size, old_size);
  assert_nvtable(tab_ref1, STATIC_HANDLE, "value", 5);
  assert_nvtable(tab_ref2, STATIC_HANDLE, "value", 5);

  nv_table_unref(tab_ref1);
  nv_table_unref(tab_ref2);
}

Test(nvtable, test_nvtable_unset_values)
{
  NVTable *tab;
  gssize size = 9999;
  const gchar *value;
  gboolean success;

  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 1024);
  value = nv_table_get_value(tab, DYN_HANDLE, &size);
  cr_assert_not_null(value);
  cr_assert_eq(value[0], 0);
  cr_assert_eq(size, 0);

  size = 1;
  value = nv_table_get_value_if_set(tab, DYN_HANDLE, &size);
  cr_assert_null(value);
  cr_assert_eq(size, 0);

  success = nv_table_add_value(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME), "foo", 3, NULL);
  cr_assert(success);
  size = 1;
  value = nv_table_get_value_if_set(tab, DYN_HANDLE, &size);
  cr_assert_not_null(value);
  cr_assert_arr_eq(value, "foo", 3);
  cr_assert_eq(size, 3);

  nv_table_unset_value(tab, DYN_HANDLE);
  size = 1;
  value = nv_table_get_value_if_set(tab, DYN_HANDLE, &size);
  cr_assert_null(value);
  cr_assert_eq(size, 0);

  nv_table_unref(tab);
}

Test(nvtable, test_nvtable_unset_copies_indirect_references)
{
  NVTable *tab;
  gssize size = 9999;
  const gchar *value;
  const gchar *indirect_nv_name = "indirect-name";

  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 1024);
  nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, strlen(DYN_NAME), "static-foo", 10, NULL);
  nv_table_add_value_indirect(tab, DYN_HANDLE, indirect_nv_name, strlen(indirect_nv_name),
                              &(NVReferencedSlice)
  {
    STATIC_HANDLE, 1, 5, 0
  },
  NULL);

  value = nv_table_get_value(tab, DYN_HANDLE, &size);
  cr_assert_not_null(value);
  cr_assert(strncmp(value, "tatic", 5) == 0);
  cr_assert_eq(size, 5);

  nv_table_unset_value(tab, STATIC_HANDLE);

  value = nv_table_get_value(tab, DYN_HANDLE, &size);
  cr_assert_not_null(value);
  cr_assert(strncmp(value, "tatic", 5) == 0);
  cr_assert_eq(size, 5);

  nv_table_unref(tab);
}
