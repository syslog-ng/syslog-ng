/*
 * Copyright (c) 2007-2009 Balabit
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
#include "serialize.h"
#include "apphook.h"

TestSuite(serialize, .init = app_startup, .fini = app_shutdown);

Test(serialize, test_serialize)
{
  GString *stream = g_string_new("");
  GString *value = g_string_new("");
  gchar buf[256];
  guint32 num = 0;

  SerializeArchive *a = serialize_string_archive_new(stream);

  serialize_write_blob(a, "MAGIC", 5);
  serialize_write_uint32(a, 0xdeadbeaf);
  serialize_write_cstring(a, "kismacska", -1);
  serialize_write_cstring(a, "tarkabarka", 10);

  serialize_archive_free(a);

  a = serialize_string_archive_new(stream);

  serialize_read_blob(a, buf, 5);
  cr_assert_arr_eq(buf, "MAGIC", 5);

  serialize_read_uint32(a, &num);
  cr_assert_eq(num, 0xdeadbeaf);

  serialize_read_string(a, value);
  cr_assert_str_eq(value->str, "kismacska");

  serialize_read_string(a, value);
  cr_assert_str_eq(value->str, "tarkabarka");
}
