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

#include <criterion/criterion.h>
#include "apphook.h"
#include "logmsg/logmsg.h"
#include "logmsg/timestamp-serialize.h"

SerializeArchive *sa = NULL;
GString *stream = NULL;
UnixTime input_timestamps[LM_TS_MAX];
UnixTime output_timestamps[LM_TS_MAX];

Test(template_timestamp, test_normal_working)
{
  cr_assert(timestamp_serialize(sa, input_timestamps), "Failed to serialize timestamps");
  cr_assert(timestamp_deserialize(sa, output_timestamps), "Failed to deserialize timestamps");

  cr_assert_str_eq((const gchar *)input_timestamps, (const gchar *)output_timestamps,
                   "The serialized and the deserialized timestamps are not equal");
}

Test(template_timestamp, test_derializing_injured_timestamp)
{
  cr_assert(timestamp_serialize(sa, input_timestamps), "Failed to serialize timestamps");

  g_string_truncate(stream, 0);
  cr_assert_not(timestamp_deserialize(sa, output_timestamps), "Should be failed");

  serialize_archive_free(sa);
  sa = serialize_string_archive_new(stream);
  cr_assert(timestamp_serialize(sa, input_timestamps), "Failed to serialize timestamps");
  g_string_truncate(stream, sizeof(guint64));
  cr_assert_not(timestamp_deserialize(sa, output_timestamps), "Should be failed");

  serialize_archive_free(sa);
  sa = serialize_string_archive_new(stream);
  cr_assert(timestamp_serialize(sa, input_timestamps), "Failed to serialize timestamps");
  g_string_truncate(stream, sizeof(guint64) + sizeof(guint32));
  cr_assert_not(timestamp_deserialize(sa, output_timestamps), "Should be failed");
}

static void
setup(void)
{
  stream = g_string_new("");

  app_startup();
  input_timestamps[LM_TS_STAMP].ut_sec = 1;
  input_timestamps[LM_TS_STAMP].ut_usec = 2;
  input_timestamps[LM_TS_STAMP].ut_gmtoff = 3;
  input_timestamps[LM_TS_RECVD].ut_sec = 4;
  input_timestamps[LM_TS_RECVD].ut_usec = 5;
  input_timestamps[LM_TS_RECVD].ut_gmtoff = 6;
  input_timestamps[LM_TS_PROCESSED].ut_sec = 255;
  input_timestamps[LM_TS_PROCESSED].ut_usec = 255;
  input_timestamps[LM_TS_PROCESSED].ut_gmtoff = -1;

  sa = serialize_string_archive_new(stream);
}

static void
teardown(void)
{
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);

  app_shutdown();
}

TestSuite(template_timestamp, .init = setup, .fini = teardown);
