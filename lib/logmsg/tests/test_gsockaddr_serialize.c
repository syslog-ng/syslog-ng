/*
 * Copyright (c) 2002-2015 Balabit
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

#include "testutils.h"
#include "apphook.h"
#include "logmsg/gsockaddr-serialize.h"

static void
test_empty()
{
  GString *stream = g_string_new("");

  SerializeArchive *sa = serialize_string_archive_new(stream);

  GSockAddr *read_addr = NULL;

  assert_true(g_sockaddr_serialize(sa, NULL), "FAILED TO SERIALIZE");
  assert_true(g_sockaddr_deserialize(sa, &read_addr), "FAILED TO READ BACK");

  assert_null(read_addr, "Should be null pointer");

  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

static void
test_inet()
{
  GSockAddr *addr = g_sockaddr_inet_new("127.0.0.1", 5555);
  GSockAddr *read_addr = NULL;

  GString *stream = g_string_new("");

  SerializeArchive *sa = serialize_string_archive_new(stream);

  assert_true(g_sockaddr_serialize(sa, addr), "FAILED TO SERIALIZE");
  assert_true(g_sockaddr_deserialize(sa, &read_addr), "FAILED TO READ BACK");

  assert_nstring((const gchar *)g_sockaddr_inet_get_sa(addr), addr->salen,
                 (const gchar *)g_sockaddr_inet_get_sa(read_addr), read_addr->salen,
                 "Bad read struct");

  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
  g_sockaddr_unref(addr);
  g_sockaddr_unref(read_addr);
}

static void
test_inet6()
{
#if SYSLOG_NG_ENABLE_IPV6
  GSockAddr *addr = g_sockaddr_inet6_new("::1", 5555);
  GSockAddr *read_addr = NULL;

  GString *stream = g_string_new("");

  SerializeArchive *sa = serialize_string_archive_new(stream);

  assert_true(g_sockaddr_serialize(sa, addr), "FAILED TO SERIALIZE");
  assert_true(g_sockaddr_deserialize(sa, &read_addr), "FAILED TO READ BACK");

  assert_nstring((const gchar *)g_sockaddr_inet6_get_sa(addr), addr->salen,
                 (const gchar *)g_sockaddr_inet6_get_sa(read_addr), read_addr->salen,
                 "Bad read struct");

  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
  g_sockaddr_unref(addr);
  g_sockaddr_unref(read_addr);
#endif
}

static void
test_unix()
{
  GSockAddr *addr = g_sockaddr_unix_new("testpath");
  GSockAddr *read_addr = NULL;

  GString *stream = g_string_new("");

  SerializeArchive *sa = serialize_string_archive_new(stream);

  assert_true(g_sockaddr_serialize(sa, addr), "FAILED TO SERIALIZE");
  assert_true(g_sockaddr_deserialize(sa, &read_addr), "FAILED TO READ BACK");

  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
  g_sockaddr_unref(addr);
  g_sockaddr_unref(read_addr);
}

static void
test_inet_false()
{
  GSockAddr *addr = g_sockaddr_inet_new("127.0.0.1", 5555);
  GSockAddr *read_addr = NULL;

  GString *stream = g_string_new("");

  SerializeArchive *sa = serialize_string_archive_new(stream);

  assert_true(g_sockaddr_serialize(sa, addr), "FAILED TO SERIALIZE");

  g_string_truncate(stream, 0);
  assert_false(g_sockaddr_deserialize(sa, &read_addr), "SHOULD HAVE FAILED HERE");

  serialize_archive_free(sa);
  sa = serialize_string_archive_new(stream);
  assert_true(g_sockaddr_serialize(sa, addr), "FAILED TO SERIALIZE");
  g_string_truncate(stream, 2);
  assert_false(g_sockaddr_deserialize(sa, &read_addr), "SHOULD BE FAILED_HERE");


  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
  g_sockaddr_unref(addr);
  g_sockaddr_unref(read_addr);
}

static void
test_inet6_false()
{
#if ENABLE_IPV6
  GSockAddr *addr = g_sockaddr_inet6_new("::1", 5555);
  GSockAddr *read_addr = NULL;

  GString *stream = g_string_new("");

  SerializeArchive *sa = serialize_string_archive_new(stream);

  assert_true(g_sockaddr_serialize(sa, addr), "FAILED TO SERIALIZE");

  assert_true(g_sockaddr_serialize(sa, addr), "FAILED TO SERIALIZE");

  g_string_truncate(stream, 0);
  assert_false(g_sockaddr_deserialize(sa, &read_addr), "SHOULD HAVE FAILED HERE");

  serialize_archive_free(sa);
  sa = serialize_string_archive_new(stream);
  assert_true(g_sockaddr_serialize(sa, addr), "FAILED TO SERIALIZE");
  g_string_truncate(stream, 2);
  assert_false(g_sockaddr_deserialize(sa, &read_addr), "SHOULD BE FAILED_HERE");

  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
  g_sockaddr_unref(addr);
  g_sockaddr_unref(read_addr);
#endif
}

static void
test_bad_family()
{
  GSockAddr *addr = g_sockaddr_inet_new("127.0.0.1", 5555);
  GSockAddr *read_addr = NULL;

  guint16 bad_family = 0xFFFF;

  GString *stream = g_string_new("");

  SerializeArchive *sa = serialize_string_archive_new(stream);

  assert_true(g_sockaddr_serialize(sa, addr), "FAILED TO SERIALIZE");

  g_string_overwrite(stream, 0, (const gchar *)&bad_family);
  assert_false(g_sockaddr_deserialize(sa, &read_addr), "SHOULD HAVE FAILED HERE");

  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
  g_sockaddr_unref(addr);
  g_sockaddr_unref(read_addr);
}

int
main(int argc, char **argv)
{
  app_startup();

  test_empty();

  test_inet();
  test_inet6();
  test_unix();

  start_grabbing_messages();
  test_inet_false();
  test_inet6_false();
  test_bad_family();
  stop_grabbing_messages();

  app_shutdown();
  return 0;
}

