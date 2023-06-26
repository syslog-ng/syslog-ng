/*
 * Copyright (c) 2023 Attila Szakacs
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


#include "otel-protobuf-formatter.hpp"
#include "otel-protobuf-parser.hpp"

#include "compat/cpp-start.h"
#include "apphook.h"
#include "cfg.h"
#include "compat/cpp-end.h"

#include <criterion/criterion.h>

using namespace syslogng::grpc::otel;

using namespace opentelemetry::proto::resource::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::logs::v1;

static LogMessage *
_create_log_msg_with_dummy_resource_and_scope()
{
  LogMessage *msg = log_msg_new_empty();

  log_msg_set_value_by_name_with_type(msg, ".otel.resource.attributes.attr_0", "val_0", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.resource.dropped_attributes_count", "1", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.resource.schema_url", "resource_schema_url", -1, LM_VT_STRING);

  log_msg_set_value_by_name_with_type(msg, ".otel.scope.name", "scope", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.scope.version", "v1.2.3", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.scope.attributes.attr_1", "val_1", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.scope.dropped_attributes_count", "2", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.scope.schema_url", "scope_schema_url", -1, LM_VT_STRING);

  return msg;
}

static void
_assert_dummy_resource_and_scope(const Resource &resource, const std::string &resource_schema_url,
                                 const InstrumentationScope &scope, const std::string &scope_schema_url)
{
  cr_assert_eq(resource.attributes_size(), 1, "%d");
  cr_assert_str_eq(resource.attributes(0).key().c_str(), "attr_0");
  cr_assert_str_eq(resource.attributes(0).value().string_value().c_str(), "val_0");
  cr_assert_eq(resource.dropped_attributes_count(), 1);
  cr_assert_str_eq(resource_schema_url.c_str(), "resource_schema_url");

  cr_assert_str_eq(scope.name().c_str(), "scope");
  cr_assert_str_eq(scope.version().c_str(), "v1.2.3");
  cr_assert_eq(scope.attributes_size(), 1);
  cr_assert_str_eq(scope.attributes(0).key().c_str(), "attr_1");
  cr_assert_str_eq(scope.attributes(0).value().string_value().c_str(), "val_1");
  cr_assert_eq(scope.dropped_attributes_count(), 2);
  cr_assert_str_eq(scope_schema_url.c_str(), "scope_schema_url");
}

Test(otel_protobuf_formatter, get_message_type)
{
  LogMessage *msg = log_msg_new_empty();
  cr_assert_eq(get_message_type(msg), MessageType::UNKNOWN);

  log_msg_set_value_by_name_with_type(msg, ".otel_raw.type", "log", -1, LM_VT_BYTES);
  cr_assert_eq(get_message_type(msg), MessageType::UNKNOWN);

  log_msg_set_value_by_name_with_type(msg, ".otel_raw.type", "log", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::LOG);

  log_msg_set_value_by_name_with_type(msg, ".otel_raw.type", "metric", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::METRIC);

  log_msg_set_value_by_name_with_type(msg, ".otel_raw.type", "span", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::SPAN);

  log_msg_set_value_by_name_with_type(msg, ".otel_raw.type", "almafa", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::UNKNOWN);

  log_msg_unset_value_by_name(msg, ".otel_raw.type");

  log_msg_set_value_by_name_with_type(msg, ".otel.type", "log", -1, LM_VT_BYTES);
  cr_assert_eq(get_message_type(msg), MessageType::UNKNOWN);

  log_msg_set_value_by_name_with_type(msg, ".otel.type", "log", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::LOG);

  log_msg_set_value_by_name_with_type(msg, ".otel.type", "metric", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::METRIC);

  log_msg_set_value_by_name_with_type(msg, ".otel.type", "span", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::SPAN);

  log_msg_set_value_by_name_with_type(msg, ".otel.type", "almafa", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::UNKNOWN);

  log_msg_unref(msg);
}

Test(otel_protobuf_formatter, get_metadata)
{
  ProtobufFormatter formatter(configuration);
  LogMessage *msg = _create_log_msg_with_dummy_resource_and_scope();

  Resource resource;
  std::string resource_schema_url;
  InstrumentationScope scope;
  std::string scope_schema_url;
  cr_assert(formatter.get_metadata(msg, resource, resource_schema_url, scope, scope_schema_url));
  log_msg_unref(msg);

  _assert_dummy_resource_and_scope(resource, resource_schema_url, scope, scope_schema_url);

  /* Raw */
  msg = _create_log_msg_with_dummy_resource_and_scope();
  ProtobufParser::store_raw_metadata(msg, "", resource, resource_schema_url, scope, scope_schema_url);

  Resource resource_from_raw;
  std::string resource_schema_url_from_raw;
  InstrumentationScope scope_from_raw;
  std::string scope_schema_url_from_raw;
  cr_assert(formatter.get_metadata(msg, resource_from_raw, resource_schema_url_from_raw, scope_from_raw,
                                   scope_schema_url_from_raw));
  log_msg_unref(msg);

  _assert_dummy_resource_and_scope(resource_from_raw, resource_schema_url_from_raw, scope_from_raw,
                                   scope_schema_url_from_raw);
}

static void
_log_record_tc_asserts(const LogRecord &log_record)
{
  cr_assert_eq(log_record.time_unix_nano(), 123);
  cr_assert_eq(log_record.observed_time_unix_nano(), 456);
  cr_assert_eq(log_record.severity_number(), SeverityNumber::SEVERITY_NUMBER_ERROR);
  cr_assert_str_eq(log_record.severity_text().c_str(), "my_error_text");
  cr_assert_eq(log_record.body().value_case(), AnyValue::kStringValue);
  cr_assert_str_eq(log_record.body().string_value().c_str(), "string_body");

  auto &attributes = log_record.attributes();
  cr_assert_eq(log_record.attributes_size(), 8);
  cr_assert_str_eq(attributes.at(0).key().c_str(), "a_string_key");
  cr_assert_eq(attributes.at(0).value().value_case(), AnyValue::kStringValue);
  cr_assert_str_eq(attributes.at(0).value().string_value().c_str(), "string");
  cr_assert_str_eq(attributes.at(1).key().c_str(), "b_int_key");
  cr_assert_eq(attributes.at(1).value().value_case(), AnyValue::kIntValue);
  cr_assert_eq(attributes.at(1).value().int_value(), 42);
  cr_assert_str_eq(attributes.at(2).key().c_str(), "c_double_key");
  cr_assert_eq(attributes.at(2).value().value_case(), AnyValue::kDoubleValue);
  cr_assert_float_eq(attributes.at(2).value().double_value(), 42.123456, std::numeric_limits<double>::epsilon());
  cr_assert_str_eq(attributes.at(3).key().c_str(), "d_bool_key");
  cr_assert_eq(attributes.at(3).value().value_case(), AnyValue::kBoolValue);
  cr_assert_eq(attributes.at(3).value().bool_value(), true);
  cr_assert_str_eq(attributes.at(4).key().c_str(), "e_null_key");
  cr_assert_eq(attributes.at(4).value().value_case(), AnyValue::VALUE_NOT_SET);
  cr_assert_str_eq(attributes.at(5).key().c_str(), "f_bytes_key");
  cr_assert_eq(attributes.at(5).value().value_case(), AnyValue::kBytesValue);
  cr_assert_eq(attributes.at(5).value().bytes_value().length(), 4);
  cr_assert_eq(memcmp(attributes.at(5).value().bytes_value().c_str(), "\0\1\2\3", 4), 0);
  cr_assert_str_eq(attributes.at(6).key().c_str(), "g_protobuf_kv_list_key");
  cr_assert_eq(attributes.at(6).value().value_case(), AnyValue::kKvlistValue);
  cr_assert_eq(attributes.at(6).value().kvlist_value().values_size(), 2);
  cr_assert_str_eq(attributes.at(6).value().kvlist_value().values().at(0).key().c_str(), "kv_1");
  cr_assert_str_eq(attributes.at(6).value().kvlist_value().values().at(0).value().string_value().c_str(), "kv_1_val");
  cr_assert_str_eq(attributes.at(6).value().kvlist_value().values().at(1).key().c_str(), "kv_2");
  cr_assert_str_eq(attributes.at(6).value().kvlist_value().values().at(1).value().string_value().c_str(), "kv_2_val");
  cr_assert_str_eq(attributes.at(7).key().c_str(), "h_protobuf_array_key");
  cr_assert_eq(attributes.at(7).value().value_case(), AnyValue::kArrayValue);
  cr_assert_eq(attributes.at(7).value().array_value().values_size(), 2);
  cr_assert_eq(attributes.at(7).value().array_value().values().at(0).int_value(), 1337);
  cr_assert_eq(attributes.at(7).value().array_value().values().at(1).int_value(), 7331);

  cr_assert_eq(log_record.dropped_attributes_count(), 11);
  cr_assert_eq(log_record.flags(), 22);
  cr_assert_eq(log_record.trace_id().length(), 16);
  cr_assert_eq(memcmp(log_record.trace_id().c_str(), "\0\1\2\3\4\5\6\7\0\1\2\3\4\5\6\7", 16), 0);
  cr_assert_eq(log_record.span_id().length(), 8);
  cr_assert_eq(memcmp(log_record.span_id().c_str(), "\0\1\2\3\4\5\6\7", 8), 0);
}

/* This testcase also tests the the handling of different types of KeyValues and implicitly AnyValues. */
Test(otel_protobuf_formatter, log_record)
{
  ProtobufFormatter formatter(configuration);
  LogMessage *msg = _create_log_msg_with_dummy_resource_and_scope();

  log_msg_set_value_by_name_with_type(msg, ".otel.log.time_unix_nano", "123", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.log.observed_time_unix_nano", "456", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.log.severity_number", "17", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.log.severity_text", "my_error_text", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.log.body", "string_body", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.log.attributes.a_string_key", "string", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.log.attributes.b_int_key", "42", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.log.attributes.c_double_key", "42.123456", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.log.attributes.d_bool_key", "true", -1, LM_VT_BOOLEAN);
  log_msg_set_value_by_name_with_type(msg, ".otel.log.attributes.e_null_key", "", -1, LM_VT_NULL);
  log_msg_set_value_by_name_with_type(msg, ".otel.log.attributes.f_bytes_key", "\0\1\2\3", 4, LM_VT_BYTES);

  AnyValue any_value_for_kv_list;
  KeyValueList *kv_list = any_value_for_kv_list.mutable_kvlist_value();
  KeyValue *kv_1 = kv_list->add_values();
  kv_1->set_key("kv_1");
  kv_1->mutable_value()->set_string_value("kv_1_val");
  KeyValue *kv_2 = kv_list->add_values();
  kv_2->set_key("kv_2");
  kv_2->mutable_value()->set_string_value("kv_2_val");
  std::string any_value_kv_list_serialized = any_value_for_kv_list.SerializeAsString();
  log_msg_set_value_by_name_with_type(msg, ".otel.log.attributes.g_protobuf_kv_list_key",
                                      any_value_kv_list_serialized.c_str(), any_value_kv_list_serialized.length(),
                                      LM_VT_PROTOBUF);

  AnyValue any_value_for_array;
  ArrayValue *array = any_value_for_array.mutable_array_value();
  AnyValue *array_value_1 = array->add_values();
  array_value_1->set_int_value(1337);
  AnyValue *array_value_2 = array->add_values();
  array_value_2->set_int_value(7331);
  std::string any_value_array_serialized = any_value_for_array.SerializeAsString();
  log_msg_set_value_by_name_with_type(msg, ".otel.log.attributes.h_protobuf_array_key",
                                      any_value_array_serialized.c_str(), any_value_array_serialized.length(),
                                      LM_VT_PROTOBUF);

  log_msg_set_value_by_name_with_type(msg, ".otel.log.dropped_attributes_count", "11", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.log.flags", "22", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.log.trace_id", "\0\1\2\3\4\5\6\7\0\1\2\3\4\5\6\7", 16, LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, ".otel.log.span_id", "\0\1\2\3\4\5\6\7", 8, LM_VT_BYTES);

  LogRecord log_record;
  cr_assert(formatter.format(msg, log_record));
  log_msg_unref(msg);

  _log_record_tc_asserts(log_record);

  /* Raw */
  msg = _create_log_msg_with_dummy_resource_and_scope();
  ProtobufParser::store_raw(msg, log_record);

  LogRecord log_record_from_raw;
  cr_assert(formatter.format(msg, log_record_from_raw));
  log_msg_unref(msg);

  _log_record_tc_asserts(log_record);
}

Test(otel_protobuf_formatter, log_record_fallback)
{
  ProtobufFormatter formatter(configuration);
  LogMessage *msg = log_msg_new_empty();

  msg->timestamps[LM_TS_STAMP] = (UnixTime)
  {
    .ut_sec = 111, .ut_usec = 222
  };
  msg->timestamps[LM_TS_RECVD] = (UnixTime)
  {
    .ut_sec = 333, .ut_usec = 444
  };
  msg->pri = LOG_USER | LOG_ERR;
  log_msg_set_value_by_name_with_type(msg, "MESSAGE", "string_body", -1, LM_VT_STRING);

  LogRecord log_record;
  formatter.format_fallback(msg, log_record);
  log_msg_unref(msg);

  cr_assert_eq(log_record.time_unix_nano(), 111000222000);
  cr_assert_eq(log_record.observed_time_unix_nano(), 333000444000);
  cr_assert_eq(log_record.severity_number(), SeverityNumber::SEVERITY_NUMBER_ERROR);
  cr_assert_eq(log_record.severity_text().length(), 0);
  cr_assert_eq(log_record.body().value_case(), AnyValue::kStringValue);
  cr_assert_str_eq(log_record.body().string_value().c_str(), "string_body");

  cr_assert_eq(log_record.attributes_size(), 0);
  cr_assert_eq(log_record.dropped_attributes_count(), 0);
  cr_assert_eq(log_record.flags(), 0);
  cr_assert_eq(log_record.trace_id().length(), 0);
  cr_assert_eq(log_record.span_id().length(), 0);
}

void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
}

void
teardown(void)
{
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(otel_protobuf_formatter, .init = setup, .fini = teardown);
