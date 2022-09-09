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

#define RAW_MSG "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [exampleSDID@0 iut=\"3\" eventSource=\"Application\"] An application event log entry..."

#define ERROR_MSG "Failed at %s(%d)", __FILE__, __LINE__

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


ParameterizedTestParameters(logmsg_serialize, test_deserialization_of_legacy_messages)
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

  return cr_make_param_array(struct iovec, messages, G_N_ELEMENTS(messages));
}


Test(logmsg_serialize, test_deserialization_of_pe_message)
{
  LogMessage *msg = _deserialize_message_from_string(serialized_pe_msg, sizeof(serialized_pe_msg));
  _check_deserialized_message_original_fields(msg);
  log_msg_unref(msg);
}

ParameterizedTest(struct iovec *param, logmsg_serialize, test_deserialization_of_legacy_messages)
{
  LogMessage *msg = _deserialize_message_from_string(param->iov_base, param->iov_len);
  _check_deserialized_message_all_fields(msg);

  log_msg_unref(msg);
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
