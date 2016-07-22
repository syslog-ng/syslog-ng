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

#define PATHUTILS_TESTCASE(testfunc, ...)  { testcase_begin("%s(%s)", #testfunc, #__VA_ARGS__); testfunc(__VA_ARGS__); testcase_end(); }

static void
test_get_filename_extension(void)
{
  assert_string(get_filename_extension("test.csv"), "csv", "test.csv, expected extension: csv");
  assert_string(get_filename_extension(".test.csv"), "csv", ".test.csv, expected extension: csv");
  assert_string(get_filename_extension("test.csv.orig"), "orig", "test.csv.orig, expected extension: orig");
  assert_string(get_filename_extension("test.csv~"), "csv~", "test.csv~, expected extension:csv~");
  assert_string(get_filename_extension("1.x") , "x", "x, expected extension is x");
  assert_true(get_filename_extension("filename") == NULL, "filename, expected extension is NULL");
  assert_true(get_filename_extension("") == NULL, "input is empty, expected extension is NULL");
  assert_true(get_filename_extension(".config") == NULL, ".config, expected extension is NULL");
  assert_true(get_filename_extension(".") == NULL, ". , expected extension is NULL");
  assert_true(get_filename_extension("...") == NULL, "..., expected extension is NULL");
  assert_true(get_filename_extension("1.") == NULL, "1., expected extension is NULL");
  assert_true(get_filename_extension(NULL) == NULL, "input is NULL, expected extension is NULL");
}

int main(int argc, char **argv)
{
  PATHUTILS_TESTCASE(test_get_filename_extension);

  return 0;
}
