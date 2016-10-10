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

#include "testutils.h"
#include "pathutils.h"
#include <criterion/criterion.h>


typedef struct _PathUtilsTestCase
{
  const gchar *filename;
  const gchar *expected_extension;
} PathUtilsTestCase;


void
_assert_filename_with_extension(PathUtilsTestCase c)
{
  const gchar *actual_filename_extension = get_filename_extension(c.filename);
  cr_assert_str_eq(actual_filename_extension, c.expected_extension,
                   "Filename: %s, Expected extension: %s, actual extension: %s", c.filename, c.expected_extension,
                   actual_filename_extension);
}

void
_assert_filename_without_extension(const gchar *filename)
{
  const gchar *actual_filename_extension = get_filename_extension(filename);
  cr_assert_null(actual_filename_extension, "No extension is expected. Filename: %s", filename);
}

Test(pathutils, test_get_filename_extension)
{
  PathUtilsTestCase test_cases[] =
  {
    {"test.csv", "csv"},
    {".test.csv", "csv"},
    {"test.csv.orig", "orig"},
    {"test.csv~", "csv~"},
    {"1.x", "x"},
  };
  gint i, nr_of_cases;

  nr_of_cases = sizeof(test_cases) / sizeof(test_cases[0]);
  for (i = 0; i < nr_of_cases; i++)
    _assert_filename_with_extension(test_cases[i]);

}

Test(pathutils, test_get_filename_extension_without_extension)
{
  gchar *test_cases[] =
  {
    "filename",
    "",
    ".config",
    ".",
    "...",
    "1.",
    NULL,
  };
  gint i, nr_of_cases;

  nr_of_cases = sizeof(test_cases) / sizeof(test_cases[0]);
  for (i = 0; i < nr_of_cases; i++)
    _assert_filename_without_extension(test_cases[i]);
}
