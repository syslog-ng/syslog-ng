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

#include "serialize.h"
#include "apphook.h"
#include <string.h>

#define TEST_ASSERT(x)  \
  do { \
   if (!(x)) \
     { \
       fprintf(stderr, "test assertion failed: " #x " line: %d\n", __LINE__); \
       return 1; \
     } \
  } while (0)

int
main()
{
  GString *stream;
  GString *value;
  SerializeArchive *a;
  gchar buf[256];
  guint32 num;

  app_startup();

  stream = g_string_new("");
  value = g_string_new("");
  a = serialize_string_archive_new(stream);

  serialize_write_blob(a, "MAGIC", 5);
  serialize_write_uint32(a, 0xdeadbeaf);
  serialize_write_cstring(a, "kismacska", -1);
  serialize_write_cstring(a, "tarkabarka", 10);

  serialize_archive_free(a);

  a = serialize_string_archive_new(stream);
  serialize_read_blob(a, buf, 5);
  TEST_ASSERT(memcmp(buf, "MAGIC", 5) == 0);
  serialize_read_uint32(a, &num);
  TEST_ASSERT(num == 0xdeadbeaf);
  serialize_read_string(a, value);
  TEST_ASSERT(strcmp(value->str, "kismacska") == 0);
  serialize_read_string(a, value);
  TEST_ASSERT(strcmp(value->str, "tarkabarka") == 0);

  app_shutdown();
  return 0;
}
