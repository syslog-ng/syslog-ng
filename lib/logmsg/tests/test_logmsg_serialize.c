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

#include "logmsg/logmsg.h"
#include "msg-format.h"
#include "stopwatch.h"
#include "apphook.h"
#include "cfg.h"
#include "plugin.h"
#include "logmsg/logmsg-serialize.h"
#include <criterion/criterion.h>

GlobalConfig *cfg;

#define RAW_MSG "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [exampleSDID@0 iut=\"3\" eventSource=\"Application\"] An application event log entry..."

#define ERROR_MSG "Failed at %s(%d)", __FILE__, __LINE__

MsgFormatOptions parse_options;

unsigned char _serialized_pe_msg[] =
{
  0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0x43, 0xfd,
  0x0f, 0x00, 0x02, 0x61, 0x60, 0x00, 0x00, 0x0e, 0x10, 0x00, 0x00, 0x00,
  0x00, 0x56, 0xa2, 0x1e, 0xb7, 0x00, 0x0e, 0x89, 0x04, 0x00, 0x00, 0x0e,
  0x10, 0x00, 0x00, 0x00, 0x00, 0x56, 0xa2, 0x1e, 0xb7, 0x00, 0x0e, 0x89,
  0x04, 0x00, 0x00, 0x0e, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x04, 0x08, 0x00, 0x00, 0x01, 0x94, 0x00, 0x00, 0x01,
  0x95, 0x00, 0x00, 0x01, 0x96, 0x00, 0x00, 0x01, 0x97, 0x32, 0x54, 0x56,
  0x4e, 0x00, 0x00, 0x00, 0x01, 0xd4, 0x00, 0x00, 0x01, 0x4c, 0x00, 0x05,
  0x09, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
  0x30, 0x00, 0x00, 0x00, 0x94, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x93, 0x00, 0x00, 0x01, 0x4c, 0x00, 0x00, 0x01,
  0x94, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x01, 0x95, 0x00, 0x00, 0x00,
  0x58, 0x00, 0x00, 0x01, 0x96, 0x00, 0x00, 0x00, 0xbc, 0x00, 0x00, 0x01,
  0x97, 0x00, 0x00, 0x00, 0xf8, 0xFC, 0x03, 0x00, 0x00, 0x1c, 0x00, 0x00,
  0x00, 0x0a, 0x00, 0x00, 0x00, 0x61, 0x61, 0x61, 0x00, 0x74, 0x65, 0x73,
  0x74, 0x5f, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x38, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x41, 0x6e,
  0x20, 0x61, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e,
  0x20, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x20, 0x6c, 0x6f, 0x67, 0x20, 0x65,
  0x6e, 0x74, 0x72, 0x79, 0x2e, 0x2e, 0x2e, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x3c, 0x00, 0x00,
  0x00, 0x0b, 0x00, 0x00, 0x00, 0x2e, 0x53, 0x44, 0x41, 0x54, 0x41, 0x2e,
  0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x53, 0x44, 0x49, 0x44, 0x40,
  0x30, 0x2e, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x53, 0x6f, 0x75, 0x72, 0x63,
  0x65, 0x00, 0x41, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f,
  0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x28, 0x00, 0x00,
  0x00, 0x01, 0x00, 0x00, 0x00, 0x2e, 0x53, 0x44, 0x41, 0x54, 0x41, 0x2e,
  0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x53, 0x44, 0x49, 0x44, 0x40,
  0x30, 0x2e, 0x69, 0x75, 0x74, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x20, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x65, 0x76,
  0x6e, 0x74, 0x73, 0x6c, 0x6f, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00,
  0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x6d, 0x79, 0x6d, 0x61, 0x63, 0x68,
  0x69, 0x6e, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x00,
  0x00, 0x2c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2e, 0x53, 0x44,
  0x41, 0x54, 0x41, 0x2e, 0x74, 0x69, 0x6d, 0x65, 0x51, 0x75, 0x61, 0x6c,
  0x69, 0x74, 0x79, 0x2e, 0x74, 0x7a, 0x4b, 0x6e, 0x6f, 0x77, 0x6e, 0x00,
  0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x2c, 0x00, 0x00,
  0x00, 0x01, 0x00, 0x00, 0x00, 0x2e, 0x53, 0x44, 0x41, 0x54, 0x41, 0x2e,
  0x74, 0x69, 0x6d, 0x65, 0x51, 0x75, 0x61, 0x6c, 0x69, 0x74, 0x79, 0x2e,
  0x69, 0x73, 0x53, 0x79, 0x6e, 0x63, 0x65, 0x64, 0x00, 0x30, 0x00, 0x00,
  0x00
};
unsigned int _serialized_pe_msg_len = sizeof(_serialized_pe_msg);

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
_check_deserialized_message(LogMessage *msg, SerializeArchive *sa)
{
  GString *output = g_string_new("");
  LogTemplate *template = log_template_new(cfg, NULL);
  log_template_compile(template, "${ISODATE}", NULL);

  cfg->template_options.frac_digits = 3;
  LogTemplateEvalOptions options = {&cfg->template_options, LTZ_SEND, 0, NULL};
  log_template_append_format(template, msg, &options, output);
  cr_assert_str_eq(output->str, "2006-10-29T01:59:59.156+01:00", ERROR_MSG);

  cr_assert_str_eq(log_msg_get_value(msg, LM_V_HOST, NULL), "mymachine", ERROR_MSG);
  cr_assert_str_eq(log_msg_get_value(msg, LM_V_PROGRAM, NULL), "evntslog", ERROR_MSG);
  cr_assert_str_eq(log_msg_get_value(msg, LM_V_MESSAGE, NULL), "An application event log entry...", ERROR_MSG);
  cr_assert_null(log_msg_get_value_if_set(msg, log_msg_get_value_handle("unset_value"), NULL), ERROR_MSG);
  cr_assert_str_eq(log_msg_get_value_by_name(msg, ".SDATA.exampleSDID@0.eventSource", NULL), "Application", ERROR_MSG);
  cr_assert_eq(msg->pri, 132, ERROR_MSG);
  log_template_unref(template);
  g_string_free(output, TRUE);
}

static LogMessage *
_create_message_to_be_serialized(const gchar *raw_msg, const int raw_msg_len)
{
  parse_options.flags |= LP_SYSLOG_PROTOCOL;
  NVHandle test_handle = log_msg_get_value_handle("aaa");

  LogMessage *msg = log_msg_new(raw_msg, raw_msg_len, &parse_options);
  log_msg_set_value(msg, test_handle, "test_value", -1);

  NVHandle indirect_handle = log_msg_get_value_handle("indirect_1");
  log_msg_set_value_indirect(msg, indirect_handle, test_handle, 5, 3);

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

Test(logmsg_serialize, serialize)
{
  NVHandle indirect_handle = 0;
  gssize length = 0;
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

  _check_deserialized_message(msg, sa);

  indirect_handle = log_msg_get_value_handle("indirect_1");
  const gchar *indirect_value = log_msg_get_value(msg, indirect_handle, &length);
  cr_assert(0==strncmp(indirect_value, "value", length), ERROR_MSG);

  log_msg_unref(msg);
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

Test(logmsg_serialize, pe_serialized_message)
{
  GString serialized = {0};
  serialized.allocated_len = 0;
  serialized.len = _serialized_pe_msg_len;
  serialized.str = (gchar *)_serialized_pe_msg;
  LogMessage *msg = log_msg_new_empty();

  SerializeArchive *sa = serialize_string_archive_new(&serialized);
  _reset_log_msg_registry();

  cr_assert(log_msg_deserialize(msg, sa), ERROR_MSG);

  _check_deserialized_message(msg, sa);

  log_msg_unref(msg);
  serialize_archive_free(sa);
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

static void
setup(void)
{
  app_startup();
  cfg = cfg_new_snippet();
  cfg_load_module(cfg, "syslogformat");
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, cfg);
}

static void
teardown(void)
{
  cfg_free(cfg);
  app_shutdown();
}

TestSuite(logmsg_serialize, .init = setup, .fini = teardown);
