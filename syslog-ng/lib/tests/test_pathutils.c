/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Viktor Tusa <tusa@balabit.hu>
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

#include "pathutils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <criterion/criterion.h>

Test(test_pathutils, test_is_file_directory)
{
  int fd = open("test.file", O_CREAT | O_RDWR, 0644);
  cr_assert_not(fd < 0, "open error");
  ssize_t done = write(fd, "a", 1);
  cr_assert_eq(done, 1, "write error");
  int error = close(fd);
  cr_assert_not(error, "close error");

  cr_assert_not(is_file_directory("test.file"), "File is not a directory!");
  cr_assert(is_file_directory("./"), "File is a directory!");

  error = unlink("test.file");
  cr_assert_not(error, "unlink error");
}

Test(test_pathutils, test_is_regular)
{
  int fd = open("test2.file", O_CREAT | O_RDWR, 0644);
  int error = close(fd);
  cr_assert_not(error, "close error");

  cr_assert(is_file_regular("test2.file"), "this is a regular file");
  cr_assert_not(is_file_regular("./"), "this is not a regular file");

  error = unlink("test2.file");
  cr_assert_not(error, "unlink error");
}

Test(test_pathutils, test_is_file_device)
{
  cr_assert(is_file_device("/dev/null"), "not recognized device file");
}

Test(test_pathutils, test_find_file_in_path)
{
  gchar *file;

  file = find_file_in_path("/dev", "null", G_FILE_TEST_EXISTS);
  cr_assert_str_eq(file, "/dev/null");
  g_free(file);

  file = find_file_in_path("/home:/dev:/root", "null", G_FILE_TEST_EXISTS);
  cr_assert_str_eq(file, "/dev/null");
  g_free(file);
}

Test(test_pathutils, test_get_filename_extension)
{
  cr_assert_str_eq(get_filename_extension("test.foo"), "foo", "wrong file name extension returned");
  cr_assert_str_eq(get_filename_extension("/test/test.foo.bar"), "bar", "wrong file name extension returned");
  cr_assert_str_eq(get_filename_extension("/test/.test/test.foo.bar"), "bar", "wrong file name extension returned");

  cr_assert_null(get_filename_extension("/test"), "wrong file name extension returned");
  cr_assert_null(get_filename_extension("test."), "wrong file name extension returned");
  cr_assert_null(get_filename_extension(""), "wrong file name extension returned");
  cr_assert_null(get_filename_extension(NULL), "wrong file name extension returned");
}
