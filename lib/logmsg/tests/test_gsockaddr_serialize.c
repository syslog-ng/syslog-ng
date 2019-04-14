/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 2015 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "apphook.h"
#include "logmsg/gsockaddr-serialize.h"

#include <string.h>

TestSuite(gsockaddr_serialize, .init = app_startup, .fini = app_shutdown);

Test(gsockaddr_serialize, test_empty)
{
  GString *stream = g_string_new("");
  SerializeArchive *sa = serialize_string_archive_new(stream);
  GSockAddr *read_addr = NULL;

  cr_assert(g_sockaddr_serialize(sa, NULL), "failed to serialize empty GSockAddr");
  cr_assert(g_sockaddr_deserialize(sa, &read_addr), "failed to read back empty GSockAddr");
  cr_assert_null(read_addr, "deserialized GSockAddr should be empty (NULL)");

  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(gsockaddr_serialize, test_inet)
{
  GSockAddr *addr = g_sockaddr_inet_new("127.0.0.1", 5555);
  GSockAddr *read_addr = NULL;
  GString *stream = g_string_new("");
  SerializeArchive *sa = serialize_string_archive_new(stream);

  cr_assert(g_sockaddr_serialize(sa, addr), "failed to serialize inet GSockAddr");
  cr_assert(g_sockaddr_deserialize(sa, &read_addr), "failed to read back inet GSockAddr");

  cr_assert_arr_eq(g_sockaddr_inet_get_sa(addr), g_sockaddr_inet_get_sa(read_addr), addr->salen);

  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
  g_sockaddr_unref(addr);
  g_sockaddr_unref(read_addr);
}

#if SYSLOG_NG_ENABLE_IPV6
Test(gsockaddr_serialize, test_inet6)
{
  GSockAddr *addr = g_sockaddr_inet6_new("::1", 5555);
  GSockAddr *read_addr = NULL;
  GString *stream = g_string_new("");
  SerializeArchive *sa = serialize_string_archive_new(stream);

  cr_assert(g_sockaddr_serialize(sa, addr), "failed to serialize inet6 GSockAddr");
  cr_assert(g_sockaddr_deserialize(sa, &read_addr), "failed to read back inet6 GSockAddr");

  cr_assert_arr_eq(g_sockaddr_inet6_get_sa(addr), g_sockaddr_inet6_get_sa(read_addr), addr->salen);

  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
  g_sockaddr_unref(addr);
  g_sockaddr_unref(read_addr);
}
#endif

Test(gsockaddr_serialize, test_unix)
{
  GSockAddr *addr = g_sockaddr_unix_new("testpath");
  GSockAddr *read_addr = NULL;
  GString *stream = g_string_new("");
  SerializeArchive *sa = serialize_string_archive_new(stream);

  cr_assert(g_sockaddr_serialize(sa, addr), "failed to serialize unix GSockAddr");
  cr_assert(g_sockaddr_deserialize(sa, &read_addr), "failed to read back unix GSockAddr");

  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
  g_sockaddr_unref(addr);
  g_sockaddr_unref(read_addr);
}

Test(gsockaddr_serialize, test_inet_false)
{
  GSockAddr *addr = g_sockaddr_inet_new("127.0.0.1", 5555);
  GSockAddr *read_addr = NULL;
  GString *stream = g_string_new("");
  SerializeArchive *sa = serialize_string_archive_new(stream);

  cr_assert(g_sockaddr_serialize(sa, addr), "failed to serialize inet GSockAddr");

  g_string_truncate(stream, 0);
  cr_assert_not(g_sockaddr_deserialize(sa, &read_addr), "SHOULD HAVE FAILED HERE");
  serialize_archive_free(sa);

  sa = serialize_string_archive_new(stream);
  cr_assert(g_sockaddr_serialize(sa, addr), "failed to serialize inet GSockAddr");
  g_string_truncate(stream, 2);
  cr_assert_not(g_sockaddr_deserialize(sa, &read_addr), "SHOULD BE FAILED_HERE");

  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
  g_sockaddr_unref(addr);
  g_sockaddr_unref(read_addr);
}

#if defined (ENABLE_IPV6)
Test(gsockaddr_serialize, test_inet6_false)
{
  GSockAddr *addr = g_sockaddr_inet6_new("::1", 5555);
  GSockAddr *read_addr = NULL;
  GString *stream = g_string_new("");
  SerializeArchive *sa = serialize_string_archive_new(stream);

  cr_assert(g_sockaddr_serialize(sa, addr), "failed to serialize inet6 GSockAddr");

  g_string_truncate(stream, 0);
  cr_assert_not(g_sockaddr_deserialize(sa, &read_addr), "SHOULD HAVE FAILED HERE");
  serialize_archive_free(sa);

  sa = serialize_string_archive_new(stream);
  cr_assert(g_sockaddr_serialize(sa, addr), "failed to serialize inet6 GSockAddr");
  g_string_truncate(stream, 2);
  cr_assert_not(g_sockaddr_deserialize(sa, &read_addr), "SHOULD BE FAILED_HERE");

  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
  g_sockaddr_unref(addr);
  g_sockaddr_unref(read_addr);
}
#endif

Test(gsockaddr_serialize, test_bad_family)
{
  GSockAddr *addr = g_sockaddr_inet_new("127.0.0.1", 5555);
  GSockAddr *read_addr = NULL;
  guint16 bad_family = 0xFFFF;
  GString *stream = g_string_new("");
  SerializeArchive *sa = serialize_string_archive_new(stream);

  cr_assert(g_sockaddr_serialize(sa, addr), "failed to serialize GSockAddr");

  g_string_overwrite_len(stream, 0, (const gchar *)&bad_family, sizeof(bad_family)/sizeof(gchar));
  cr_assert_not(g_sockaddr_deserialize(sa, &read_addr), "SHOULD HAVE FAILED HERE");

  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
  g_sockaddr_unref(addr);
  g_sockaddr_unref(read_addr);
}
