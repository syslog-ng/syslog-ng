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
#include "logmsg/logmsg.h"
#include "logmsg/timestamp-serialize.h"

#define PREPARE_TEST LogStamp input_timestamps[LM_TS_MAX]; \
  LogStamp output_timestamps[LM_TS_MAX]; \
  GString *stream = g_string_new(""); \
  input_timestamps[LM_TS_STAMP].tv_sec = 1; \
  input_timestamps[LM_TS_STAMP].tv_usec = 2; \
  input_timestamps[LM_TS_STAMP].zone_offset = 3; \
  input_timestamps[LM_TS_RECVD].tv_sec = 4; \
  input_timestamps[LM_TS_RECVD].tv_usec = 5; \
  input_timestamps[LM_TS_RECVD].zone_offset = 6; \
  input_timestamps[LM_TS_PROCESSED].tv_sec = 255; \
  input_timestamps[LM_TS_PROCESSED].tv_usec = 255; \
  input_timestamps[LM_TS_PROCESSED].zone_offset = LOGSTAMP_ZONE_OFFSET_UNSET; \
  SerializeArchive *sa = serialize_string_archive_new(stream);

#define CLEAN_TEST serialize_archive_free(sa); \
  g_string_free(stream, TRUE);

static void
test_normal_working(void)
{
  PREPARE_TEST
  assert_true(timestamp_serialize(sa, input_timestamps), "Failed to serialize timestamps");
  assert_true(timestamp_deserialize(sa, output_timestamps), "Failed to deserialize timestamps");

  assert_nstring((const gchar *)input_timestamps, sizeof(input_timestamps),
                 (const gchar *)output_timestamps, sizeof(output_timestamps),
                 "The serialized and the deserialized timestamps are not equal");

  CLEAN_TEST
}

static void
test_derializing_injured_timestamp(void)
{
  PREPARE_TEST

  assert_true(timestamp_serialize(sa, input_timestamps), "Failed to serialize timestamps");

  g_string_truncate(stream, 0);
  assert_false(timestamp_deserialize(sa, output_timestamps), "Should be failed");

  serialize_archive_free(sa);
  sa = serialize_string_archive_new(stream);
  assert_true(timestamp_serialize(sa, input_timestamps), "Failed to serialize timestamps");
  g_string_truncate(stream, sizeof(guint64));
  assert_false(timestamp_deserialize(sa, output_timestamps), "Should be failed");

  serialize_archive_free(sa);
  sa = serialize_string_archive_new(stream);
  assert_true(timestamp_serialize(sa, input_timestamps), "Failed to serialize timestamps");
  g_string_truncate(stream, sizeof(guint64) + sizeof(guint32));
  assert_false(timestamp_deserialize(sa, output_timestamps), "Should be failed");

  CLEAN_TEST
}

int
main(int argc, char **argv)
{
  app_startup();

  test_normal_working();

  start_grabbing_messages();
  test_derializing_injured_timestamp();
  stop_grabbing_messages();

  app_shutdown();
  return 0;
}
