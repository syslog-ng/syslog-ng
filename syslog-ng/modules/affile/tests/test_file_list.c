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

#include <criterion/criterion.h>

#include "file-list.h"


TestSuite(hashed_queue, .init = NULL, .fini = NULL);


Test(hashed_queue, normal)
{
  PendingFileList *queue = pending_file_list_new();
  pending_file_list_add(queue, "file1");
  pending_file_list_add(queue, "file2");
  pending_file_list_add(queue, "file3");


  gchar *f1 = pending_file_list_pop(queue);
  gchar *f2 = pending_file_list_pop(queue);
  gchar *f3 = pending_file_list_pop(queue);
  cr_assert_str_eq(f1, "file1");
  cr_assert_str_eq(f2, "file2");
  cr_assert_str_eq(f3, "file3");

  g_free(f1);
  g_free(f2);
  g_free(f3);
  pending_file_list_free(queue);
}

Test(hashed_queue, delete_first)
{
  PendingFileList *queue = pending_file_list_new();
  pending_file_list_add(queue, "file1");
  pending_file_list_add(queue, "file2");
  pending_file_list_add(queue, "file3");

  cr_assert_eq(pending_file_list_remove(queue, "file1"), TRUE);
  gchar *f2 = pending_file_list_pop(queue);
  gchar *f3 = pending_file_list_pop(queue);
  cr_assert_str_eq(f2, "file2");
  cr_assert_str_eq(f3, "file3");

  g_free(f2);
  g_free(f3);
  pending_file_list_free(queue);
}

Test(hashed_queue, delete_middle)
{
  PendingFileList *queue = pending_file_list_new();
  pending_file_list_add(queue, "file1");
  pending_file_list_add(queue, "file2");
  pending_file_list_add(queue, "file3");

  cr_assert_eq(pending_file_list_remove(queue, "file2"), TRUE);
  gchar *f1 = pending_file_list_pop(queue);
  gchar *f3 = pending_file_list_pop(queue);
  cr_assert_str_eq(f1, "file1");
  cr_assert_str_eq(f3, "file3");

  g_free(f1);
  g_free(f3);
  pending_file_list_free(queue);
}

Test(hashed_queue, delete_last)
{
  PendingFileList *queue = pending_file_list_new();
  pending_file_list_add(queue, "file1");
  pending_file_list_add(queue, "file2");
  pending_file_list_add(queue, "file3");

  cr_assert_eq(pending_file_list_remove(queue, "file3"), TRUE);
  gchar *f1 = pending_file_list_pop(queue);
  gchar *f2 = pending_file_list_pop(queue);
  gchar *f3 = pending_file_list_pop(queue);
  cr_assert_str_eq(f1, "file1");
  cr_assert_str_eq(f2, "file2");
  cr_assert_eq(f3, NULL);

  g_free(f1);
  g_free(f2);
  pending_file_list_free(queue);
}

Test(hashed_queue, delete_non_existent)
{
  PendingFileList *queue = pending_file_list_new();
  pending_file_list_add(queue, "file1");
  pending_file_list_add(queue, "file2");
  pending_file_list_add(queue, "file3");

  cr_assert_eq(pending_file_list_remove(queue, "file4"), FALSE);
  gchar *f1 = pending_file_list_pop(queue);
  gchar *f2 = pending_file_list_pop(queue);
  gchar *f3 = pending_file_list_pop(queue);
  cr_assert_str_eq(f1, "file1");
  cr_assert_str_eq(f2, "file2");
  cr_assert_str_eq(f3, "file3");

  g_free(f1);
  g_free(f2);
  g_free(f3);
  pending_file_list_free(queue);
}

Test(hashed_queue, no_duplication)
{
  PendingFileList *queue = pending_file_list_new();
  pending_file_list_add(queue, "file1");
  pending_file_list_add(queue, "file2");
  pending_file_list_add(queue, "file3");
  pending_file_list_add(queue, "file1");

  gchar *f1 = pending_file_list_pop(queue);
  gchar *f2 = pending_file_list_pop(queue);
  gchar *f3 = pending_file_list_pop(queue);
  cr_assert(pending_file_list_pop(queue) == NULL);
  cr_assert_str_eq(f1, "file1");
  cr_assert_str_eq(f2, "file2");
  cr_assert_str_eq(f3, "file3");

  g_free(f1);
  g_free(f2);
  g_free(f3);
  pending_file_list_free(queue);
}
