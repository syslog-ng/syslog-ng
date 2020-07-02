/*
 * Copyright (c) 2020 Balabit
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

#include "qdisk.h"

#include <criterion/criterion.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <errno.h>

Test(test_read_all, basic)
{
  gchar file_name[] = "test_for_pread_all.txt";
  gchar text[] = "012";
  g_file_set_contents(file_name, text, sizeof(text), NULL);

  int fd = open(file_name, O_RDONLY);
  cr_assert(fd > 0);

  gchar buffer[sizeof(text)];
  gsize from = 1;
  gsize data_to_read = sizeof(text)-from;

  ssize_t result = pread_all(fd, buffer, data_to_read, from, 1);
  cr_assert_eq(result, data_to_read, "wrong number of bytes read: %lu != %lu: errno: %d",
               result, data_to_read, errno);

  cr_assert_str_eq("12", buffer);

  close(fd);
  g_remove(file_name);
}
