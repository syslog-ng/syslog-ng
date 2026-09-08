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
#include <criterion/parameterized.h>
#include "libtest/msg_parse_lib.h"
#include "libtest/cr_template.h"
#include "libtest/stopwatch.h"

#include "logmsg/logmsg.h"
#include "msg-format.h"
#include "apphook.h"
#include "cfg.h"
#include "plugin.h"
#include "logmsg/logmsg-serialize.h"
#include "logmsg/nvtable.h"
#include "logmsg/nvtable-serialize.h"
#include "logmsg/nvtable-serialize-legacy.h"

#define RAW_MSG "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [exampleSDID@0 iut=\"3\" eventSource=\"Application\"] An application event log entry..."

#define ERROR_MSG "Failed at %s(%d)", __FILE__, __LINE__

#define TEST_ONE 1
#define TEST_NO_ENTRIES 0
#define TEST_NV_TABLE_SIZE (sizeof(NVTable) + sizeof(guint32) + (2 * sizeof(NVEntry)))
#define TEST_SMALL_NV_TABLE_SIZE 48
#define TEST_NV_TABLE_USED 8
#define TEST_INVALID_NV_TABLE_USED (TEST_SMALL_NV_TABLE_SIZE + TEST_ONE)
#define TEST_INVALID_ENTRY_OFFSET TEST_ONE
#define TEST_INVALID_NV_TABLE_SIZE (sizeof(NVTable) - TEST_ONE)
/* v22 sizes are encoded in four-byte units. */
#define TEST_V22_SIZE 16
#define TEST_V22_USED 2
#define TEST_V22_ENTRY_OFFSET 3
#define TEST_V22_PAYLOAD_SIZE (TEST_V22_USED * sizeof(guint32))
/* These headers are intentionally truncated malformed records. */
#define TEST_TRUNCATED_LEGACY_HEADER_SIZE 8
#define TEST_LEGACY_HEADER_SIZE_WITH_STATIC 10
#define TEST_LEGACY_HEADER_SIZE_WITH_DYNAMIC (TEST_LEGACY_STATIC_ENTRIES_OFFSET + (2 * sizeof(guint32)))
#define TEST_LEGACY_PAYLOAD_SIZE TEST_V22_PAYLOAD_SIZE
#define TEST_LEGACY_TABLE_SIZE (2 * TEST_LEGACY_HEADER_SIZE_WITH_DYNAMIC)
#define TEST_LEGACY_USED 3
#define TEST_LEGACY_ENTRY_FLAGS_OFFSET 0
#define TEST_LEGACY_ENTRY_ALLOC_LEN_OFFSET 2
#define TEST_LEGACY_ENTRY_VALUE_LEN_OFFSET (TEST_LEGACY_ENTRY_ALLOC_LEN_OFFSET + sizeof(guint16))
#define TEST_LEGACY_ENTRY_REFERENCE_OFFSET (TEST_LEGACY_ENTRY_VALUE_LEN_OFFSET + sizeof(guint16))
#define TEST_LEGACY_ENTRY_REFERENCE_LENGTH_OFFSET (TEST_LEGACY_ENTRY_REFERENCE_OFFSET + sizeof(guint16))
#define TEST_LEGACY_SIZE_OFFSET 0
#define TEST_LEGACY_USED_OFFSET 2
#define TEST_LEGACY_DYNAMIC_COUNT_OFFSET 4
#define TEST_LEGACY_STATIC_COUNT_OFFSET 6
#define TEST_LEGACY_STATIC_ENTRIES_OFFSET 8
#define TEST_LEGACY_ENTRY_INDIRECT_FLAG TEST_ONE
#define TEST_LEGACY_ENTRY_ALLOC_LEN TEST_LEGACY_USED
#define TEST_LEGACY_HANDLE TEST_ONE
#define TEST_LEGACY_REFERENCE_OFFSET (TEST_LEGACY_PAYLOAD_SIZE + TEST_V22_USED)
#define TEST_LEGACY_ENTRY_REFERENCE_LENGTH TEST_LEGACY_REFERENCE_OFFSET
#define TEST_LEGACY_DYNAMIC_ENTRIES_OFFSET TEST_LEGACY_STATIC_ENTRIES_OFFSET
#define TEST_BENCHMARK_TABLE_SIZE 4096
#define TEST_BENCHMARK_ITERATIONS 100000
#define TEST_BENCHMARK_ENTRY_CAPACITY 16
#define TEST_BENCHMARK_ENTRY_COUNT (2 * TEST_BENCHMARK_ENTRY_CAPACITY)
#define TEST_BENCHMARK_NAME_SIZE 32
#define TEST_BENCHMARK_VALUE_LENGTH (sizeof("value") - TEST_ONE)
#define TEST_FIRST_HANDLE TEST_ONE
#define TEST_ZERO_ALLOCATION_SIZE 0

MsgFormatOptions parse_options;

static void
_alloc_dummy_values_to_change_handle_values_across_restarts(void)
{
  static gint iteration = 1;

  for (gint i = 0; i < iteration; i++)
    {
      gchar dummy_name[32];

      g_snprintf(dummy_name, sizeof(dummy_name), "dummy%d", i);
      nv_registry_alloc_handle(logmsg_registry, dummy_name);
    }
  iteration *= 2;
}

static void
_reset_log_msg_registry(void)
{
  log_msg_registry_deinit();
  log_msg_registry_init();
  _alloc_dummy_values_to_change_handle_values_across_restarts();
}

static void
_check_deserialized_message_original_fields(LogMessage *msg)
{
  assert_template_format_msg("${ISODATE}", "2006-10-29T01:59:59.156+01:00", msg);

  assert_log_message_value_and_type(msg, LM_V_HOST, "mymachine", LM_VT_STRING);
  assert_log_message_value_and_type(msg, LM_V_PROGRAM, "evntslog", LM_VT_STRING);
  assert_log_message_value_and_type(msg, LM_V_MESSAGE, "An application event log entry...", LM_VT_STRING);
  assert_log_message_value_unset(msg, log_msg_get_value_handle("unset_value"));

  assert_log_message_value_and_type(msg,
                                    log_msg_get_value_handle(".SDATA.exampleSDID@0.eventSource"),
                                    "Application",
                                    LM_VT_STRING);
  cr_assert_eq(msg->pri, 132, ERROR_MSG);

}

static void
_check_deserialized_message_all_fields(LogMessage *msg)
{
  _check_deserialized_message_original_fields(msg);
  assert_log_message_value(msg, log_msg_get_value_handle("indirect_1"), "val");
  assert_log_message_value_and_type(msg, log_msg_get_value_handle("indirect_2"), "53", LM_VT_INTEGER);

}

static LogMessage *
_create_message_to_be_serialized(const gchar *raw_msg, const int raw_msg_len)
{
  parse_options.flags |= LP_SYSLOG_PROTOCOL;
  NVHandle test_handle = log_msg_get_value_handle("aaa");

  LogMessage *msg = msg_format_parse(&parse_options, (const guchar *) raw_msg, raw_msg_len);
  log_msg_set_value(msg, test_handle, "test_value53", -1);

  NVHandle indirect_handle = log_msg_get_value_handle("indirect_1");
  log_msg_set_value_indirect(msg, indirect_handle, test_handle, 5, 3);
  NVHandle indirect_with_type_handle = log_msg_get_value_handle("indirect_2");
  log_msg_set_value_indirect_with_type(msg, indirect_with_type_handle, test_handle, 10, 2, LM_VT_INTEGER);

  log_msg_set_value_by_name(msg, "unset_value", "foobar", -1);
  log_msg_unset_value_by_name(msg, "unset_value");

  for (int i = 0; i < 32; i++)
    {
      gchar value_name[64];

      g_snprintf(value_name, sizeof(value_name), ".SDATA.dynamic.field%d", i);
      log_msg_set_value_by_name(msg, value_name, "value", -1);

      g_snprintf(value_name, sizeof(value_name), ".normal.dynamic.field%d", i);
      log_msg_set_value_by_name(msg, value_name, "value", -1);
    }

  return msg;
}

static SerializeArchive *
_serialize_message_for_test(GString *stream, const gchar *raw_msg)
{
  SerializeArchive *sa = serialize_string_archive_new(stream);

  LogMessage *msg = _create_message_to_be_serialized(raw_msg, strlen(raw_msg));
  log_msg_serialize(msg, sa, 0);
  log_msg_unref(msg);
  return sa;
}

static LogMessage *
_deserialize_message_from_string(const guint8 *serialized, gsize serialized_len)
{
  GString s = {0};

  s.allocated_len = 0;
  s.len = serialized_len;
  s.str = (gchar *) serialized;
  LogMessage *msg = log_msg_new_empty();

  SerializeArchive *sa = serialize_string_archive_new(&s);
  _reset_log_msg_registry();

  cr_assert(log_msg_deserialize(msg, sa), ERROR_MSG);
  serialize_archive_free(sa);
  return msg;
}

Test(logmsg_serialize, serialize)
{
  GString *stream = g_string_new("");

  SerializeArchive *sa = _serialize_message_for_test(stream, RAW_MSG);
  _reset_log_msg_registry();
  LogMessage *msg = log_msg_new_empty();

  cr_assert(log_msg_deserialize(msg, sa), ERROR_MSG);

  /* we use nv_registry_get_handle() as it will not change the name-value
   * pair flags, whereas log_msg_get_value_handle() would */
  NVHandle sdata_handle = nv_registry_get_handle(logmsg_registry, ".SDATA.exampleSDID@0.eventSource");
  cr_assert(sdata_handle != 0,
            "the .SDATA.exampleSDID@0.eventSource handle was not defined during deserialization");
  cr_assert(log_msg_is_handle_sdata(sdata_handle),
            "deserialized SDATA name-value pairs have to marked as such");

  _check_deserialized_message_all_fields(msg);

  log_msg_unref(msg);
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_invalid_nvtable_layout)
{
  NVTable table =
  {
    .size = TEST_SMALL_NV_TABLE_SIZE,
    .used = TEST_NV_TABLE_USED,
    .index_size = G_MAXUINT16,
  };

  cr_assert_not(nv_table_alloc_check(&table, TEST_NO_ENTRIES, TRUE));

  table.index_size = TEST_NO_ENTRIES;
  table.used = table.size;
  cr_assert_not(nv_table_alloc_check(&table, TEST_NO_ENTRIES, TRUE));
}

static SerializeArchive *
_create_nvtable_archive(GString **stream)
{
  *stream = g_string_new("");
  return serialize_string_archive_new(*stream);
}

static void
_write_nvtable_metadata(SerializeArchive *sa)
{
  guint32 magic;

  memcpy(&magic, NV_TABLE_MAGIC_V2, sizeof(magic));
  serialize_write_uint32(sa, magic);
  serialize_write_uint8(sa, G_BYTE_ORDER == G_BIG_ENDIAN ? NVT_SF_BE : TEST_NO_ENTRIES);
}

static void
_write_current_nvtable_with_entry(SerializeArchive *sa, guint32 ofs, NVEntry *entry)
{
  _write_nvtable_metadata(sa);
  serialize_write_uint32(sa, TEST_NV_TABLE_SIZE);
  serialize_write_uint32(sa, sizeof(NVEntry));
  serialize_write_uint16(sa, TEST_NO_ENTRIES);
  serialize_write_uint8(sa, TEST_ONE);
  serialize_write_uint32(sa, ofs);
  serialize_write_blob(sa, entry, sizeof(NVEntry));
}

static void
_write_current_nvtable_with_dynamic_entry(SerializeArchive *sa, guint32 ofs, NVEntry *entry)
{
  _write_nvtable_metadata(sa);
  serialize_write_uint32(sa, TEST_NV_TABLE_SIZE);
  serialize_write_uint32(sa, sizeof(NVEntry));
  serialize_write_uint16(sa, TEST_ONE);
  serialize_write_uint8(sa, TEST_NO_ENTRIES);
  serialize_write_uint32(sa, TEST_ONE);
  serialize_write_uint32(sa, ofs);
  serialize_write_blob(sa, entry, sizeof(NVEntry));
}

Test(logmsg_serialize, reject_malformed_current_nvtable)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);
  LogMessageSerializationState state = { .sa = sa };

  _write_nvtable_metadata(sa);
  serialize_write_uint32(sa, TEST_SMALL_NV_TABLE_SIZE);
  serialize_write_uint32(sa, TEST_NO_ENTRIES);
  serialize_write_uint16(sa, G_MAXUINT16);
  serialize_write_uint8(sa, 0);
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize(&state));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_current_nvtable_smaller_than_header)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);
  LogMessageSerializationState state = { .sa = sa };

  _write_nvtable_metadata(sa);
  serialize_write_uint32(sa, TEST_INVALID_NV_TABLE_SIZE);
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize(&state));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_current_nvtable_with_used_outside_allocation)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);
  LogMessageSerializationState state = { .sa = sa };

  _write_nvtable_metadata(sa);
  serialize_write_uint32(sa, TEST_SMALL_NV_TABLE_SIZE);
  serialize_write_uint32(sa, TEST_INVALID_NV_TABLE_USED);
  serialize_write_uint16(sa, TEST_NO_ENTRIES);
  serialize_write_uint8(sa, TEST_NO_ENTRIES);
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize(&state));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_current_nvtable_entry_offset_outside_payload)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);
  LogMessageSerializationState state = { .sa = sa };
  NVEntry entry = { .alloc_len = sizeof(NVEntry) };

  _write_current_nvtable_with_entry(sa, TEST_INVALID_ENTRY_OFFSET, &entry);
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize(&state));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_current_nvtable_entry_size_outside_payload)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);
  LogMessageSerializationState state = { .sa = sa };
  NVEntry entry = { .alloc_len = sizeof(NVEntry) + TEST_ONE };

  _write_current_nvtable_with_entry(sa, sizeof(NVEntry), &entry);
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize(&state));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_current_dynamic_entry_offset_outside_payload)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);
  LogMessageSerializationState state = { .sa = sa };
  NVEntry entry = { .alloc_len = sizeof(NVEntry) };

  _write_current_nvtable_with_dynamic_entry(sa, TEST_INVALID_ENTRY_OFFSET, &entry);
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize(&state));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_current_nvtable_entry_value_outside_allocation)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);
  LogMessageSerializationState state = { .sa = sa };
  NVEntry entry =
  {
    .alloc_len = sizeof(NVEntry),
    .vdirect.value_len = G_MAXUINT32,
  };

  _write_current_nvtable_with_entry(sa, sizeof(NVEntry), &entry);
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize(&state));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_current_indirect_entry_reference_outside_payload)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);
  LogMessageSerializationState state = { .sa = sa };
  NVEntry entry =
  {
    .indirect = TRUE,
    .alloc_len = sizeof(NVEntry),
    .vindirect.ofs = 1,
    .vindirect.len = G_MAXUINT32,
  };

  _write_current_nvtable_with_entry(sa, sizeof(NVEntry), &entry);
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize(&state));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, accept_valid_current_nvtable_entry)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);
  LogMessageSerializationState state = { .sa = sa };
  NVEntry entry = { .alloc_len = sizeof(NVEntry) };

  _write_current_nvtable_with_entry(sa, sizeof(NVEntry), &entry);
  serialize_string_archive_reset(sa);

  NVTable *table = nv_table_deserialize(&state);
  cr_assert_not_null(table);
  g_free(table);
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_malformed_v22_nvtable)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);

  _write_nvtable_metadata(sa);
  serialize_write_uint16(sa, TEST_V22_SIZE);
  serialize_write_uint16(sa, TEST_NO_ENTRIES);
  serialize_write_uint16(sa, G_MAXUINT16);
  serialize_write_uint8(sa, 0);
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize_22(sa));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_v22_nvtable_smaller_than_header)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);

  _write_nvtable_metadata(sa);
  serialize_write_uint16(sa, TEST_NO_ENTRIES);
  serialize_write_uint16(sa, TEST_NO_ENTRIES);
  serialize_write_uint16(sa, TEST_NO_ENTRIES);
  serialize_write_uint8(sa, TEST_NO_ENTRIES);
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize_22(sa));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_v22_nvtable_entry_offset_outside_payload)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);

  _write_nvtable_metadata(sa);
  serialize_write_uint16(sa, TEST_V22_SIZE);
  serialize_write_uint16(sa, TEST_V22_USED);
  serialize_write_uint16(sa, TEST_NO_ENTRIES);
  serialize_write_uint8(sa, TEST_ONE);
  serialize_write_uint16(sa, TEST_V22_ENTRY_OFFSET);
  serialize_write_blob(sa, "\0\0\0\0\0\0\0\0", TEST_V22_PAYLOAD_SIZE);
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize_22(sa));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_malformed_legacy_nvtable_header)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);

  serialize_write_uint32(sa, TEST_TRUNCATED_LEGACY_HEADER_SIZE);
  serialize_write_blob(sa, "\0\0\0\0\0\0\0\0", TEST_TRUNCATED_LEGACY_HEADER_SIZE);
  serialize_write_uint32(sa, TEST_LEGACY_USED + TEST_ONE);
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize_legacy(sa));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_legacy_nvtable_entry_offset_outside_payload)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);
  guint8 header[TEST_LEGACY_HEADER_SIZE_WITH_STATIC] = { 0 };
  guint16 value;

  value = TEST_V22_SIZE;
  memcpy(&header[TEST_LEGACY_SIZE_OFFSET], &value, sizeof(value));
  value = TEST_V22_USED;
  memcpy(&header[TEST_LEGACY_USED_OFFSET], &value, sizeof(value));
  header[TEST_LEGACY_STATIC_COUNT_OFFSET] = TEST_ONE;
  value = TEST_V22_ENTRY_OFFSET;
  memcpy(&header[TEST_LEGACY_STATIC_ENTRIES_OFFSET], &value, sizeof(value));

  serialize_write_uint32(sa, sizeof(header));
  serialize_write_blob(sa, header, sizeof(header));
  serialize_write_uint32(sa, TEST_LEGACY_PAYLOAD_SIZE);
  serialize_write_blob(sa, "\0\0\0\0\0\0\0\0", TEST_LEGACY_PAYLOAD_SIZE);
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize_legacy(sa));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_legacy_nvtable_entry_length_outside_allocation)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);
  guint8 header[TEST_LEGACY_HEADER_SIZE_WITH_STATIC] = { 0 };
  guint8 payload[TEST_LEGACY_PAYLOAD_SIZE] = { 0 };
  guint16 value;

  value = TEST_V22_SIZE;
  memcpy(&header[TEST_LEGACY_SIZE_OFFSET], &value, sizeof(value));
  value = TEST_V22_USED;
  memcpy(&header[TEST_LEGACY_USED_OFFSET], &value, sizeof(value));
  header[TEST_LEGACY_STATIC_COUNT_OFFSET] = TEST_ONE;
  value = TEST_V22_USED;
  memcpy(&header[TEST_LEGACY_STATIC_ENTRIES_OFFSET], &value, sizeof(value));

  value = TEST_V22_USED;
  memcpy(&payload[TEST_LEGACY_ENTRY_ALLOC_LEN_OFFSET], &value, sizeof(value));
  value = G_MAXUINT16;
  memcpy(&payload[TEST_LEGACY_ENTRY_VALUE_LEN_OFFSET], &value, sizeof(value));

  serialize_write_uint32(sa, sizeof(header));
  serialize_write_blob(sa, header, sizeof(header));
  serialize_write_uint32(sa, sizeof(payload));
  serialize_write_blob(sa, payload, sizeof(payload));
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize_legacy(sa));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, reject_legacy_indirect_entry_reference_outside_payload)
{
  GString *stream;
  SerializeArchive *sa = _create_nvtable_archive(&stream);
  guint8 header[TEST_LEGACY_HEADER_SIZE_WITH_DYNAMIC] = { 0 };
  guint8 payload[TEST_LEGACY_USED * sizeof(guint32)] = { 0 };
  guint16 value;
  guint32 dynamic_entry;

  value = TEST_LEGACY_TABLE_SIZE;
  memcpy(&header[TEST_LEGACY_SIZE_OFFSET], &value, sizeof(value));
  value = TEST_LEGACY_USED;
  memcpy(&header[TEST_LEGACY_USED_OFFSET], &value, sizeof(value));
  value = TEST_ONE;
  memcpy(&header[TEST_LEGACY_DYNAMIC_COUNT_OFFSET], &value, sizeof(value));
  dynamic_entry = ((guint32) TEST_LEGACY_HANDLE << 16) | TEST_V22_ENTRY_OFFSET;
  memcpy(&header[TEST_LEGACY_DYNAMIC_ENTRIES_OFFSET], &dynamic_entry, sizeof(dynamic_entry));

  payload[TEST_LEGACY_ENTRY_FLAGS_OFFSET] = TEST_LEGACY_ENTRY_INDIRECT_FLAG;
  value = TEST_LEGACY_ENTRY_ALLOC_LEN;
  memcpy(&payload[TEST_LEGACY_ENTRY_ALLOC_LEN_OFFSET], &value, sizeof(value));
  value = TEST_LEGACY_REFERENCE_OFFSET;
  memcpy(&payload[TEST_LEGACY_ENTRY_REFERENCE_OFFSET], &value, sizeof(value));
  value = TEST_LEGACY_ENTRY_REFERENCE_LENGTH;
  memcpy(&payload[TEST_LEGACY_ENTRY_REFERENCE_LENGTH_OFFSET], &value, sizeof(value));

  serialize_write_uint32(sa, sizeof(header));
  serialize_write_blob(sa, header, sizeof(header));
  serialize_write_uint32(sa, sizeof(payload));
  serialize_write_blob(sa, payload, sizeof(payload));
  serialize_string_archive_reset(sa);

  cr_assert_null(nv_table_deserialize_legacy(sa));
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

static LogMessage *
_create_message_to_be_serialized_with_ts_processed(const gchar *raw_msg, const int raw_msg_len, UnixTime *processed)
{
  LogMessage *msg = _create_message_to_be_serialized(RAW_MSG, strlen(RAW_MSG));

  msg->timestamps[LM_TS_PROCESSED].ut_sec = processed->ut_sec;
  msg->timestamps[LM_TS_PROCESSED].ut_usec = processed->ut_usec;
  msg->timestamps[LM_TS_PROCESSED].ut_gmtoff = processed->ut_gmtoff;

  return msg;
}

static void
_check_processed_timestamp(LogMessage *msg, UnixTime *processed)
{
  cr_assert_eq(msg->timestamps[LM_TS_PROCESSED].ut_sec, processed->ut_sec,
               "tv_sec value does not match");
  cr_assert_eq(msg->timestamps[LM_TS_PROCESSED].ut_usec, processed->ut_usec,
               "tv_usec value does not match");
  cr_assert_eq(msg->timestamps[LM_TS_PROCESSED].ut_gmtoff, processed->ut_gmtoff,
               "zone_offset value does not match");
}

Test(logmsg_serialize, simple_serialization)
{
  LogMessage *msg = _create_message_to_be_serialized(RAW_MSG, strlen(RAW_MSG));
  GString *stream = g_string_sized_new(512);
  SerializeArchive *sa = serialize_string_archive_new(stream);

  log_msg_serialize(msg, sa, 0);

  log_msg_unref(msg);
  msg = log_msg_new_empty();

  log_msg_deserialize(msg, sa);

  UnixTime ls =
  {
    .ut_sec = msg->timestamps[LM_TS_RECVD].ut_sec,
    .ut_usec = msg->timestamps[LM_TS_RECVD].ut_usec,
    .ut_gmtoff = msg->timestamps[LM_TS_RECVD].ut_gmtoff
  };

  _check_processed_timestamp(msg, &ls);

  log_msg_unref(msg);
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, given_ts_processed)
{
  LogMessage *msg = _create_message_to_be_serialized(RAW_MSG, strlen(RAW_MSG));
  GString *stream = g_string_sized_new(512);
  SerializeArchive *sa = serialize_string_archive_new(stream);

  UnixTime ls =
  {
    .ut_sec = 11,
    .ut_usec = 12,
    .ut_gmtoff = 13
  };

  log_msg_serialize_with_ts_processed(msg, sa, &ls, 0);

  log_msg_unref(msg);
  msg = log_msg_new_empty();

  log_msg_deserialize(msg, sa);

  _check_processed_timestamp(msg, &ls);

  log_msg_unref(msg);
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, existing_ts_processed)
{
  UnixTime ls =
  {
    .ut_sec = 1,
    .ut_usec = 2,
    .ut_gmtoff = 3
  };

  LogMessage *msg = _create_message_to_be_serialized_with_ts_processed(RAW_MSG, strlen(RAW_MSG), &ls);
  GString *stream = g_string_sized_new(512);
  SerializeArchive *sa = serialize_string_archive_new(stream);

  log_msg_serialize(msg, sa, 0);

  log_msg_unref(msg);
  msg = log_msg_new_empty();

  log_msg_deserialize(msg, sa);

  _check_processed_timestamp(msg, &ls);

  log_msg_unref(msg);
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, existing_and_given_ts_processed)
{
  UnixTime ls =
  {
    .ut_sec = 1,
    .ut_usec = 2,
    .ut_gmtoff = 3
  };

  LogMessage *msg = _create_message_to_be_serialized_with_ts_processed(RAW_MSG, strlen(RAW_MSG), &ls);
  GString *stream = g_string_sized_new(512);
  SerializeArchive *sa = serialize_string_archive_new(stream);

  ls.ut_sec = 11;
  ls.ut_usec = 12;
  ls.ut_gmtoff = 13;

  log_msg_serialize_with_ts_processed(msg, sa, &ls, 0);

  log_msg_unref(msg);
  msg = log_msg_new_empty();

  log_msg_deserialize(msg, sa);

  _check_processed_timestamp(msg, &ls);

  log_msg_unref(msg);
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

#include "messages/syslog-ng-pe-6.0-msg.h"
#include "messages/syslog-ng-3.17.1-msg.h"
#include "messages/syslog-ng-3.18.1-msg.h"
#include "messages/syslog-ng-3.21.1-msg.h"
#include "messages/syslog-ng-3.25.1-msg.h"
#include "messages/syslog-ng-3.26.1-msg.h"
#include "messages/syslog-ng-3.28.1-msg.h"
#include "messages/syslog-ng-3.29.1-msg.h"
#include "messages/syslog-ng-3.30.1-msg.h"


Test(logmsg_serialize, test_deserialization_of_pe_message)
{
  LogMessage *msg = _deserialize_message_from_string(serialized_pe_msg, sizeof(serialized_pe_msg));
  _check_deserialized_message_original_fields(msg);
  log_msg_unref(msg);
}

/* Keep this as a plain Test + loop (not ParameterizedTest + ParameterizedTestParameters)
 * the cases are pointer-based iovec entries, and we must avoid pointer payload transport through
 * Criterion parameterization on macOS.
 */
Test(logmsg_serialize, test_deserialization_of_legacy_messages)
{
  static struct iovec messages[] =
  {
    { serialized_message_3_17_1, sizeof(serialized_message_3_17_1) },
    { serialized_message_3_18_1, sizeof(serialized_message_3_18_1) },
    { serialized_message_3_21_1, sizeof(serialized_message_3_21_1) },
    { serialized_message_3_25_1, sizeof(serialized_message_3_25_1) },
    { serialized_message_3_26_1, sizeof(serialized_message_3_26_1) },
    { serialized_message_3_28_1, sizeof(serialized_message_3_28_1) },
    { serialized_message_3_29_1, sizeof(serialized_message_3_29_1) },
    { serialized_message_3_30_1, sizeof(serialized_message_3_30_1) },
  };

  for (gsize i = 0; i < G_N_ELEMENTS(messages); i++)
    {
      LogMessage *msg = _deserialize_message_from_string(messages[i].iov_base, messages[i].iov_len);
      _check_deserialized_message_all_fields(msg);
      log_msg_unref(msg);
    }
}

Test(logmsg_serialize, serialization_performance)
{
  LogMessage *msg = _create_message_to_be_serialized(RAW_MSG, strlen(RAW_MSG));
  GString *stream = g_string_sized_new(512);
  const int iterations = 100000;

  SerializeArchive *sa = serialize_string_archive_new(stream);
  start_stopwatch();
  for (int i = 0; i < iterations; i++)
    {
      g_string_truncate(stream, 0);
      log_msg_serialize(msg, sa, 0);
    }
  stop_stopwatch_and_display_result(iterations, "serializing (without compaction) %d times took", iterations);
  serialize_archive_free(sa);
  log_msg_unref(msg);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, serialization_with_compaction_performance)
{
  LogMessage *msg = _create_message_to_be_serialized(RAW_MSG, strlen(RAW_MSG));
  GString *stream = g_string_sized_new(512);
  const int iterations = 100000;

  SerializeArchive *sa = serialize_string_archive_new(stream);
  start_stopwatch();
  for (int i = 0; i < iterations; i++)
    {
      g_string_truncate(stream, 0);
      log_msg_serialize(msg, sa, LMSF_COMPACTION);
    }
  stop_stopwatch_and_display_result(iterations, "serializing (with compaction) %d times took", iterations);
  serialize_archive_free(sa);
  log_msg_unref(msg);
  g_string_free(stream, TRUE);
}

Test(logmsg_serialize, deserialization_performance)
{
  GString *stream = g_string_sized_new(512);
  SerializeArchive *sa = _serialize_message_for_test(stream, RAW_MSG);
  const int iterations = 100000;
  LogMessage *msg;

  start_stopwatch();
  for (int i = 0; i < iterations; i++)
    {
      serialize_string_archive_reset(sa);
      msg = log_msg_new_empty();
      log_msg_deserialize(msg, sa);
      log_msg_unref(msg);
    }
  stop_stopwatch_and_display_result(iterations, "deserializing %d times took", iterations);
  serialize_archive_free(sa);
  g_string_free(stream, TRUE);
}

static gboolean
_nv_table_alloc_check_fast(NVTable *self, gsize alloc_size)
{
  return nv_table_alloc_check(self, alloc_size, FALSE);
}

static gboolean
_nv_table_alloc_check_detailed(NVTable *self, gsize alloc_size)
{
  return nv_table_alloc_check(self, alloc_size, TRUE);
}

static guint64
_measure_nv_table_alloc_check(NVTable *table, gsize alloc_size, gboolean detailed, gint iterations)
{
  volatile gboolean valid = TRUE;
  guint64 elapsed;

  start_stopwatch();
  for (gint i = 0; i < iterations; i++)
    valid &= detailed ? _nv_table_alloc_check_detailed(table, alloc_size) :
             _nv_table_alloc_check_fast(table, alloc_size);
  elapsed = stop_stopwatch_and_get_result();

  cr_assert(valid);
  return elapsed;
}

Test(logmsg_serialize, nvtable_alloc_check_performance)
{
  NVTable *table = nv_table_new(TEST_BENCHMARK_ENTRY_CAPACITY,
                                TEST_BENCHMARK_ENTRY_CAPACITY,
                                TEST_BENCHMARK_TABLE_SIZE);
  const gint iterations = TEST_BENCHMARK_ITERATIONS;
  const gsize allocation_sizes[] = { TEST_ZERO_ALLOCATION_SIZE, sizeof(NVIndexEntry) };

  for (NVHandle handle = TEST_FIRST_HANDLE; handle <= TEST_BENCHMARK_ENTRY_COUNT; handle++)
    {
      gchar name[TEST_BENCHMARK_NAME_SIZE];

      g_snprintf(name, sizeof(name), "benchmark-%u", handle);
      cr_assert(nv_table_add_value(table, handle, name, strlen(name), "value",
                                   TEST_BENCHMARK_VALUE_LENGTH, TEST_NO_ENTRIES, NULL));
    }

  for (guint i = 0; i < G_N_ELEMENTS(allocation_sizes); i++)
    {
      guint64 fast_elapsed = _measure_nv_table_alloc_check(table, allocation_sizes[i], FALSE, iterations);
      guint64 detailed_elapsed = _measure_nv_table_alloc_check(table, allocation_sizes[i], TRUE, iterations);

      g_print("NVTable allocation check (%zu bytes): fast=%" G_GUINT64_FORMAT " us, "
              "detailed=%" G_GUINT64_FORMAT " us (%.2fx)\n",
              allocation_sizes[i], fast_elapsed, detailed_elapsed,
              (gdouble) detailed_elapsed / MAX(fast_elapsed, (guint64) TEST_ONE));
    }

  nv_table_unref(table);
}

static void
setup(void)
{
  app_startup();

  init_template_tests();
  configuration->template_options.frac_digits = 3;

  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);

}

static void
teardown(void)
{
  deinit_template_tests();
  app_shutdown();
}

TestSuite(logmsg_serialize, .init = setup, .fini = teardown);
