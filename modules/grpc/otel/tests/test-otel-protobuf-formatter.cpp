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
#include "otel-logmsg-handles.hpp"

#include "compat/cpp-start.h"
#include "apphook.h"
#include "cfg.h"
#include "compat/cpp-end.h"

#include <criterion/criterion.h>

using namespace syslogng::grpc::otel;

using namespace opentelemetry::proto::resource::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::logs::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::trace::v1;

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
  cr_assert_eq(resource.attributes_size(), 1);
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
  cr_assert_eq(log_record.attributes_size(), 9);
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
  cr_assert_str_eq(attributes.at(8).key().c_str(), "i_list_key");
  cr_assert_eq(attributes.at(8).value().value_case(), AnyValue::kArrayValue);
  cr_assert_eq(attributes.at(8).value().array_value().values_size(), 4);
  cr_assert_str_eq(attributes.at(8).value().array_value().values().at(0).string_value().c_str(), "a,");
  cr_assert_str_eq(attributes.at(8).value().array_value().values().at(1).string_value().c_str(), "b");
  cr_assert_str_eq(attributes.at(8).value().array_value().values().at(2).string_value().c_str(), "c,");
  cr_assert_str_eq(attributes.at(8).value().array_value().values().at(3).string_value().c_str(), "d");

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

  log_msg_set_value_by_name_with_type(msg, ".otel.log.attributes.i_list_key", "\"a,\",b,\"c,\",d", -1, LM_VT_LIST);

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

static void
_metric_common_tc_asserts(const Metric &metric)
{
  cr_assert_str_eq(metric.name().c_str(), "my_metric_name");
  cr_assert_str_eq(metric.description().c_str(), "My metric description");
  cr_assert_str_eq(metric.unit().c_str(), "my_unit");
}

Test(otel_protobuf_formatter, metric_common)
{
  ProtobufFormatter formatter(configuration);
  LogMessage *msg = _create_log_msg_with_dummy_resource_and_scope();

  log_msg_set_value_by_name_with_type(msg, ".otel.metric.name", "my_metric_name", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.description", "My metric description", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.unit", "my_unit", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.type", "gauge", -1, LM_VT_STRING);

  Metric metric;
  cr_assert(formatter.format(msg, metric));
  log_msg_unref(msg);

  _metric_common_tc_asserts(metric);

  /* Raw */
  msg = _create_log_msg_with_dummy_resource_and_scope();
  ProtobufParser::store_raw(msg, metric);

  Metric metric_from_raw;
  cr_assert(formatter.format(msg, metric_from_raw));
  log_msg_unref(msg);

  _metric_common_tc_asserts(metric_from_raw);
}

static void
_metric_gauge_tc_asserts(const Metric &metric)
{
  cr_assert_eq(metric.data_case(), Metric::kGauge);
  const Gauge &gauge = metric.gauge();

  cr_assert_eq(gauge.data_points_size(), 2);
  const NumberDataPoint &data_point_0 = gauge.data_points().at(0);
  cr_assert_eq(data_point_0.attributes_size(), 1);
  cr_assert_str_eq(data_point_0.attributes().at(0).key().c_str(), "string_key_0");
  cr_assert_str_eq(data_point_0.attributes().at(0).value().string_value().c_str(), "string_0");
  cr_assert_eq(data_point_0.start_time_unix_nano(), 123);
  cr_assert_eq(data_point_0.time_unix_nano(), 456);
  cr_assert_eq(data_point_0.value_case(), NumberDataPoint::kAsInt);
  cr_assert_eq(data_point_0.as_int(), 11);
  cr_assert_eq(data_point_0.flags(), 22);
  cr_assert_eq(data_point_0.exemplars_size(), 2);
  const Exemplar &exemplar_0 = data_point_0.exemplars().at(0);
  cr_assert_eq(exemplar_0.filtered_attributes_size(), 2);
  cr_assert_str_eq(exemplar_0.filtered_attributes().at(0).key().c_str(), "a_0");
  cr_assert_str_eq(exemplar_0.filtered_attributes().at(0).value().string_value().c_str(), "val_0");
  cr_assert_str_eq(exemplar_0.filtered_attributes().at(1).key().c_str(), "a_1");
  cr_assert_str_eq(exemplar_0.filtered_attributes().at(1).value().string_value().c_str(), "val_1");
  cr_assert_eq(exemplar_0.time_unix_nano(), 987);
  cr_assert_eq(exemplar_0.value_case(), Exemplar::kAsInt);
  cr_assert_eq(exemplar_0.as_int(), 999);
  cr_assert_eq(exemplar_0.span_id().length(), 8);
  cr_assert_eq(memcmp(exemplar_0.span_id().c_str(), "\1\1\1\1\1\1\1\1", 8), 0);
  cr_assert_eq(exemplar_0.trace_id().length(), 16);
  cr_assert_eq(memcmp(exemplar_0.trace_id().c_str(), "\2\2\2\2\2\2\2\2\2\2\2\2\2\2\2\2", 16), 0);
  const Exemplar &exemplar_1 = data_point_0.exemplars().at(1);
  cr_assert_eq(exemplar_1.filtered_attributes_size(), 1);
  cr_assert_str_eq(exemplar_1.filtered_attributes().at(0).key().c_str(), "a_0");
  cr_assert_eq(exemplar_1.filtered_attributes().at(0).value().int_value(), 42);
  cr_assert_eq(exemplar_1.time_unix_nano(), 654);
  cr_assert_eq(exemplar_1.value_case(), Exemplar::kAsDouble);
  cr_assert_float_eq(exemplar_1.as_double(), 88.88, std::numeric_limits<double>::epsilon());
  cr_assert_eq(exemplar_1.span_id().length(), 8);
  cr_assert_eq(memcmp(exemplar_1.span_id().c_str(), "\3\3\3\3\3\3\3\3", 8), 0);
  cr_assert_eq(exemplar_1.trace_id().length(), 16);
  cr_assert_eq(memcmp(exemplar_1.trace_id().c_str(), "\4\4\4\4\4\4\4\4\4\4\4\4\4\4\4\4", 16), 0);

  const NumberDataPoint &data_point_1 = gauge.data_points().at(1);
  cr_assert_eq(data_point_1.attributes_size(), 1);
  cr_assert_str_eq(data_point_1.attributes().at(0).key().c_str(), "string_key_1");
  cr_assert_str_eq(data_point_1.attributes().at(0).value().string_value().c_str(), "string_1");
  cr_assert_eq(data_point_1.start_time_unix_nano(), 111);
  cr_assert_eq(data_point_1.time_unix_nano(), 222);
  cr_assert_eq(data_point_1.value_case(), NumberDataPoint::kAsDouble);
  cr_assert_float_eq(data_point_1.as_double(), 33.33, std::numeric_limits<double>::epsilon());
  cr_assert_eq(data_point_1.flags(), 44);
}

Test(otel_protobuf_formatter, metric_gauge)
{
  ProtobufFormatter formatter(configuration);
  LogMessage *msg = _create_log_msg_with_dummy_resource_and_scope();

  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.type", "gauge", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.attributes.string_key_0", "string_0",
                                      -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.start_time_unix_nano", "123", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.time_unix_nano", "456", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.value", "11", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.exemplars.0.filtered_attributes.a_0",
                                      "val_0", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.exemplars.0.filtered_attributes.a_1",
                                      "val_1", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.exemplars.0.time_unix_nano", "987",
                                      -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.exemplars.0.value", "999", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.exemplars.0.span_id",
                                      "\1\1\1\1\1\1\1\1", 8, LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.exemplars.0.trace_id",
                                      "\2\2\2\2\2\2\2\2\2\2\2\2\2\2\2\2", 16, LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.exemplars.1.filtered_attributes.a_0",
                                      "42", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.exemplars.1.time_unix_nano", "654",
                                      -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.exemplars.1.value", "88.88", -1,
                                      LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.exemplars.1.span_id",
                                      "\3\3\3\3\3\3\3\3", 8, LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.exemplars.1.trace_id",
                                      "\4\4\4\4\4\4\4\4\4\4\4\4\4\4\4\4", 16, LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.0.flags", "22", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.1.attributes.string_key_1", "string_1",
                                      -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.1.start_time_unix_nano", "111", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.1.time_unix_nano", "222", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.1.value", "33.33", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.gauge.data_points.1.flags", "44", -1, LM_VT_INTEGER);

  Metric metric;
  cr_assert(formatter.format(msg, metric));
  log_msg_unref(msg);

  _metric_gauge_tc_asserts(metric);

  /* Raw */
  msg = _create_log_msg_with_dummy_resource_and_scope();
  ProtobufParser::store_raw(msg, metric);

  Metric metric_from_raw;
  cr_assert(formatter.format(msg, metric_from_raw));
  log_msg_unref(msg);

  _metric_gauge_tc_asserts(metric_from_raw);
}

static void
_metric_sum_tc_asserts(const Metric &metric)
{
  cr_assert_eq(metric.data_case(), Metric::kSum);
  const Sum &sum = metric.sum();

  cr_assert_eq(sum.data_points_size(), 2);
  const NumberDataPoint &data_point_0 = sum.data_points().at(0);
  cr_assert_eq(data_point_0.attributes_size(), 0);
  cr_assert_eq(data_point_0.start_time_unix_nano(), 123);
  cr_assert_eq(data_point_0.time_unix_nano(), 456);
  cr_assert_eq(data_point_0.value_case(), NumberDataPoint::kAsInt);
  cr_assert_eq(data_point_0.as_int(), 11);
  cr_assert_eq(data_point_0.flags(), 22);
  cr_assert_eq(data_point_0.exemplars_size(), 0);

  const NumberDataPoint &data_point_1 = sum.data_points().at(1);
  cr_assert_eq(data_point_1.attributes_size(), 0);
  cr_assert_eq(data_point_1.start_time_unix_nano(), 111);
  cr_assert_eq(data_point_1.time_unix_nano(), 222);
  cr_assert_eq(data_point_1.value_case(), NumberDataPoint::kAsDouble);
  cr_assert_float_eq(data_point_1.as_double(), 33.33, std::numeric_limits<double>::epsilon());
  cr_assert_eq(data_point_1.flags(), 44);

  cr_assert_eq(sum.aggregation_temporality(), AggregationTemporality::AGGREGATION_TEMPORALITY_CUMULATIVE);
  cr_assert_eq(sum.is_monotonic(), true);
}

Test(otel_protobuf_formatter, metric_sum)
{
  ProtobufFormatter formatter(configuration);
  LogMessage *msg = _create_log_msg_with_dummy_resource_and_scope();

  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.type", "sum", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.sum.data_points.0.start_time_unix_nano", "123", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.sum.data_points.0.time_unix_nano", "456", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.sum.data_points.0.value", "11", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.sum.data_points.0.flags", "22", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.sum.data_points.1.start_time_unix_nano", "111", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.sum.data_points.1.time_unix_nano", "222", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.sum.data_points.1.value", "33.33", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.sum.data_points.1.flags", "44", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.sum.aggregation_temporality", "2", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.sum.is_monotonic", "true", -1, LM_VT_BOOLEAN);

  Metric metric;
  cr_assert(formatter.format(msg, metric));
  log_msg_unref(msg);

  _metric_sum_tc_asserts(metric);

  /* Raw */
  msg = _create_log_msg_with_dummy_resource_and_scope();
  ProtobufParser::store_raw(msg, metric);

  Metric metric_from_raw;
  cr_assert(formatter.format(msg, metric_from_raw));
  log_msg_unref(msg);

  _metric_sum_tc_asserts(metric_from_raw);
}

static void
_metric_histogram_tc_asserts(const Metric &metric)
{
  cr_assert_eq(metric.data_case(), Metric::kHistogram);
  const Histogram &histogram = metric.histogram();
  cr_assert_eq(histogram.data_points_size(), 2);

  const HistogramDataPoint &data_point_0 = histogram.data_points().at(0);
  cr_assert_eq(data_point_0.attributes_size(), 1);
  cr_assert_str_eq(data_point_0.attributes().at(0).key().c_str(), "attr_0");
  cr_assert_str_eq(data_point_0.attributes().at(0).value().string_value().c_str(), "val_0");
  cr_assert_eq(data_point_0.start_time_unix_nano(), 123);
  cr_assert_eq(data_point_0.time_unix_nano(), 456);
  cr_assert_eq(data_point_0.count(), 11);
  cr_assert_float_eq(data_point_0.sum(), 4.2, std::numeric_limits<double>::epsilon());
  cr_assert_eq(data_point_0.bucket_counts_size(), 2);
  cr_assert_eq(data_point_0.bucket_counts().at(0), 22);
  cr_assert_eq(data_point_0.bucket_counts().at(1), 33);
  cr_assert_eq(data_point_0.explicit_bounds_size(), 2);
  cr_assert_float_eq(data_point_0.explicit_bounds().at(0), 44.44, std::numeric_limits<double>::epsilon());
  cr_assert_float_eq(data_point_0.explicit_bounds().at(1), 55.55, std::numeric_limits<double>::epsilon());
  cr_assert_eq(data_point_0.exemplars_size(), 1);
  cr_assert_eq(data_point_0.exemplars().at(0).filtered_attributes_size(), 1);
  cr_assert_str_eq(data_point_0.exemplars().at(0).filtered_attributes().at(0).key().c_str(), "a_0");
  cr_assert_str_eq(data_point_0.exemplars().at(0).filtered_attributes().at(0).value().string_value().c_str(), "val_0");
  cr_assert_eq(data_point_0.exemplars().at(0).time_unix_nano(), 987);
  cr_assert_eq(data_point_0.exemplars().at(0).as_int(), 999);
  cr_assert_eq(data_point_0.flags(), 22);
  cr_assert_float_eq(data_point_0.min(), 13.37, std::numeric_limits<double>::epsilon());
  cr_assert_float_eq(data_point_0.max(), 73.31, std::numeric_limits<double>::epsilon());

  const HistogramDataPoint &data_point_1 = histogram.data_points().at(1);
  cr_assert_eq(data_point_1.attributes_size(), 0);
  cr_assert_eq(data_point_1.start_time_unix_nano(), 111);
  cr_assert_eq(data_point_1.time_unix_nano(), 222);
  cr_assert_eq(data_point_1.count(), 33);
  cr_assert_float_eq(data_point_1.sum(), 4.4, std::numeric_limits<double>::epsilon());
  cr_assert_eq(data_point_1.bucket_counts_size(), 0);
  cr_assert_eq(data_point_1.explicit_bounds_size(), 0);
  cr_assert_eq(data_point_1.exemplars_size(), 0);
  cr_assert_eq(data_point_1.flags(), 55);
  cr_assert_float_eq(data_point_1.min(), 66.66, std::numeric_limits<double>::epsilon());
  cr_assert_float_eq(data_point_1.max(), 77.77, std::numeric_limits<double>::epsilon());

  cr_assert_eq(histogram.aggregation_temporality(), AggregationTemporality::AGGREGATION_TEMPORALITY_CUMULATIVE);
}

Test(otel_protobuf_formatter, metric_histogram)
{
  ProtobufFormatter formatter(configuration);
  LogMessage *msg = _create_log_msg_with_dummy_resource_and_scope();

  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.type", "histogram", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.attributes.attr_0", "val_0", -1,
                                      LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.start_time_unix_nano", "123", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.time_unix_nano", "456", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.count", "11", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.sum", "4.2", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.bucket_counts.0", "22", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.bucket_counts.1", "33", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.explicit_bounds.0", "44.44", -1,
                                      LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.explicit_bounds.1", "55.55", -1,
                                      LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg,
                                      ".otel.metric.data.histogram.data_points.0.exemplars.0.filtered_attributes.a_0",
                                      "val_0", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.exemplars.0.time_unix_nano",
                                      "987", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.exemplars.0.value", "999", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.flags", "22", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.min", "13.37", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.0.max", "73.31", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.1.start_time_unix_nano", "111", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.1.time_unix_nano", "222", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.1.count", "33", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.1.sum", "4.4", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.1.flags", "55", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.1.min", "66.66", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.data_points.1.max", "77.77", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.histogram.aggregation_temporality", "2", -1,
                                      LM_VT_INTEGER);

  Metric metric;
  cr_assert(formatter.format(msg, metric));
  log_msg_unref(msg);

  _metric_histogram_tc_asserts(metric);

  /* Raw */
  msg = _create_log_msg_with_dummy_resource_and_scope();
  ProtobufParser::store_raw(msg, metric);

  Metric metric_from_raw;
  cr_assert(formatter.format(msg, metric_from_raw));
  log_msg_unref(msg);

  _metric_histogram_tc_asserts(metric_from_raw);
}

static void
_metric_exponential_histogram_tc_asserts(const Metric &metric)
{
  cr_assert_eq(metric.data_case(), Metric::kExponentialHistogram);
  const ExponentialHistogram &exponential_histogram = metric.exponential_histogram();
  cr_assert_eq(exponential_histogram.data_points_size(), 2);

  const ExponentialHistogramDataPoint &data_point_0 = exponential_histogram.data_points().at(0);
  cr_assert_eq(data_point_0.attributes_size(), 1);
  cr_assert_str_eq(data_point_0.attributes().at(0).key().c_str(), "attr_0");
  cr_assert_str_eq(data_point_0.attributes().at(0).value().string_value().c_str(), "val_0");
  cr_assert_eq(data_point_0.start_time_unix_nano(), 123);
  cr_assert_eq(data_point_0.time_unix_nano(), 456);
  cr_assert_eq(data_point_0.count(), 555);
  cr_assert_float_eq(data_point_0.sum(), 56.7, std::numeric_limits<double>::epsilon());
  cr_assert_eq(data_point_0.scale(), 666);
  cr_assert_eq(data_point_0.zero_count(), 11);
  cr_assert_eq(data_point_0.positive().offset(), 999);
  cr_assert_eq(data_point_0.positive().bucket_counts_size(), 2);
  cr_assert_eq(data_point_0.positive().bucket_counts().at(0), 888);
  cr_assert_eq(data_point_0.positive().bucket_counts().at(1), 777);
  cr_assert_eq(data_point_0.negative().bucket_counts_size(), 2);
  cr_assert_eq(data_point_0.negative().offset(), 666);
  cr_assert_eq(data_point_0.negative().bucket_counts().at(0), 555);
  cr_assert_eq(data_point_0.negative().bucket_counts().at(1), 444);
  cr_assert_eq(data_point_0.exemplars_size(), 1);
  cr_assert_eq(data_point_0.exemplars().at(0).filtered_attributes_size(), 1);
  cr_assert_str_eq(data_point_0.exemplars().at(0).filtered_attributes().at(0).key().c_str(), "a_0");
  cr_assert_str_eq(data_point_0.exemplars().at(0).filtered_attributes().at(0).value().string_value().c_str(), "val_0");
  cr_assert_eq(data_point_0.exemplars().at(0).time_unix_nano(), 987);
  cr_assert_eq(data_point_0.exemplars().at(0).as_int(), 999);
  cr_assert_eq(data_point_0.flags(), 22);
  cr_assert_float_eq(data_point_0.min(), 13.37, std::numeric_limits<double>::epsilon());
  cr_assert_float_eq(data_point_0.max(), 73.31, std::numeric_limits<double>::epsilon());
  cr_assert_float_eq(data_point_0.zero_threshold(), 11.22, std::numeric_limits<double>::epsilon());

  const ExponentialHistogramDataPoint &data_point_1 = exponential_histogram.data_points().at(1);
  cr_assert_eq(data_point_1.attributes_size(), 0);
  cr_assert_eq(data_point_1.start_time_unix_nano(), 1111);
  cr_assert_eq(data_point_1.time_unix_nano(), 2222);
  cr_assert_eq(data_point_1.count(), 1234);
  cr_assert_float_eq(data_point_1.sum(), 567.89, std::numeric_limits<double>::epsilon());
  cr_assert_eq(data_point_1.scale(), 3333);
  cr_assert_eq(data_point_1.zero_count(), 4444);
  cr_assert_eq(data_point_1.positive().offset(), 5555);
  cr_assert_eq(data_point_1.positive().bucket_counts_size(), 1);
  cr_assert_eq(data_point_1.positive().bucket_counts().at(0), 6666);
  cr_assert_eq(data_point_1.negative().bucket_counts_size(), 1);
  cr_assert_eq(data_point_1.negative().offset(), 7777);
  cr_assert_eq(data_point_1.negative().bucket_counts().at(0), 8888);
  cr_assert_eq(data_point_1.exemplars_size(), 0);
  cr_assert_eq(data_point_1.flags(), 9999);
  cr_assert_float_eq(data_point_1.min(), 11.11, std::numeric_limits<double>::epsilon());
  cr_assert_float_eq(data_point_1.max(), 22.22, std::numeric_limits<double>::epsilon());
  cr_assert_float_eq(data_point_1.zero_threshold(), 33.33, std::numeric_limits<double>::epsilon());

  cr_assert_eq(exponential_histogram.aggregation_temporality(),
               AggregationTemporality::AGGREGATION_TEMPORALITY_CUMULATIVE);
}

Test(otel_protobuf_formatter, metric_exponential_histogram)
{
  ProtobufFormatter formatter(configuration);
  LogMessage *msg = _create_log_msg_with_dummy_resource_and_scope();

  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.type", "exponential_histogram", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.attributes.attr_0",
                                      "val_0", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.start_time_unix_nano",
                                      "123", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.time_unix_nano",
                                      "456", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.count", "555", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.sum", "56.7", -1,
                                      LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.scale", "666", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.zero_count", "11", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.positive.offset",
                                      "999", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg,
                                      ".otel.metric.data.exponential_histogram.data_points.0.positive.bucket_counts.0",
                                      "888", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg,
                                      ".otel.metric.data.exponential_histogram.data_points.0.positive.bucket_counts.1",
                                      "777", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.negative.offset",
                                      "666", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg,
                                      ".otel.metric.data.exponential_histogram.data_points.0.negative.bucket_counts.0",
                                      "555", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg,
                                      ".otel.metric.data.exponential_histogram.data_points.0.negative.bucket_counts.1",
                                      "444", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.exemplars.0."
                                      "filtered_attributes.a_0", "val_0", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.exemplars.0."
                                      "time_unix_nano", "987", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.exemplars.0.value",
                                      "999", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.flags", "22", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.min", "13.37", -1,
                                      LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.max", "73.31", -1,
                                      LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.0.zero_threshold",
                                      "11.22", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.1.start_time_unix_nano",
                                      "1111", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.1.time_unix_nano",
                                      "2222", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.1.count", "1234", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.1.sum", "567.89", -1,
                                      LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.1.scale", "3333", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.1.zero_count", "4444",
                                      -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.1.positive.offset",
                                      "5555", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg,
                                      ".otel.metric.data.exponential_histogram.data_points.1.positive.bucket_counts.0",
                                      "6666", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.1.negative.offset",
                                      "7777", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg,
                                      ".otel.metric.data.exponential_histogram.data_points.1.negative.bucket_counts.0",
                                      "8888", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.1.flags", "9999", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.1.min", "11.11", -1,
                                      LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.1.max", "22.22", -1,
                                      LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.data_points.1.zero_threshold",
                                      "33.33", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.exponential_histogram.aggregation_temporality", "2", -1,
                                      LM_VT_INTEGER);

  Metric metric;
  cr_assert(formatter.format(msg, metric));
  log_msg_unref(msg);

  _metric_exponential_histogram_tc_asserts(metric);

  /* Raw */
  msg = _create_log_msg_with_dummy_resource_and_scope();
  ProtobufParser::store_raw(msg, metric);

  Metric metric_from_raw;
  cr_assert(formatter.format(msg, metric_from_raw));
  log_msg_unref(msg);

  _metric_exponential_histogram_tc_asserts(metric_from_raw);
}

static void
_metric_summary_tc_asserts(const Metric &metric)
{
  cr_assert_eq(metric.data_case(), Metric::kSummary);
  const Summary &summary = metric.summary();
  cr_assert_eq(summary.data_points_size(), 2);

  const SummaryDataPoint &data_point_0 = summary.data_points().at(0);
  cr_assert_eq(data_point_0.attributes_size(), 1);
  cr_assert_str_eq(data_point_0.attributes().at(0).key().c_str(), "attr_0");
  cr_assert_str_eq(data_point_0.attributes().at(0).value().string_value().c_str(), "val_0");
  cr_assert_eq(data_point_0.start_time_unix_nano(), 123);
  cr_assert_eq(data_point_0.time_unix_nano(), 456);
  cr_assert_eq(data_point_0.count(), 555);
  cr_assert_float_eq(data_point_0.sum(), 56.7, std::numeric_limits<double>::epsilon());
  cr_assert_eq(data_point_0.quantile_values_size(), 2);
  cr_assert_float_eq(data_point_0.quantile_values().at(0).quantile(), 11.1, std::numeric_limits<double>::epsilon());
  cr_assert_float_eq(data_point_0.quantile_values().at(0).value(), 22.2, std::numeric_limits<double>::epsilon());
  cr_assert_float_eq(data_point_0.quantile_values().at(1).quantile(), 33.3, std::numeric_limits<double>::epsilon());
  cr_assert_float_eq(data_point_0.quantile_values().at(1).value(), 44.4, std::numeric_limits<double>::epsilon());
  cr_assert_eq(data_point_0.flags(), 22);

  const SummaryDataPoint &data_point_1 = summary.data_points().at(1);
  cr_assert_eq(data_point_1.attributes_size(), 0);
  cr_assert_eq(data_point_1.start_time_unix_nano(), 111);
  cr_assert_eq(data_point_1.time_unix_nano(), 222);
  cr_assert_eq(data_point_1.count(), 333);
  cr_assert_float_eq(data_point_1.sum(), 44.4, std::numeric_limits<double>::epsilon());
  cr_assert_eq(data_point_1.quantile_values_size(), 1);
  cr_assert_float_eq(data_point_1.quantile_values().at(0).quantile(), 55.5, std::numeric_limits<double>::epsilon());
  cr_assert_float_eq(data_point_1.quantile_values().at(0).value(), 66.6, std::numeric_limits<double>::epsilon());
  cr_assert_eq(data_point_1.flags(), 777);
}

Test(otel_protobuf_formatter, metric_summary)
{
  ProtobufFormatter formatter(configuration);
  LogMessage *msg = _create_log_msg_with_dummy_resource_and_scope();

  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.type", "summary", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.0.attributes.attr_0", "val_0", -1,
                                      LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.0.start_time_unix_nano", "123", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.0.time_unix_nano", "456", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.0.count", "555", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.0.sum", "56.7", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.0.quantile_values.0.quantile", "11.1",
                                      -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.0.quantile_values.0.value", "22.2",
                                      -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.0.quantile_values.1.quantile", "33.3",
                                      -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.0.quantile_values.1.value", "44.4",
                                      -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.0.flags", "22", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.1.start_time_unix_nano", "111", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.1.time_unix_nano", "222", -1,
                                      LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.1.count", "333", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.1.sum", "44.4", -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.1.quantile_values.0.quantile", "55.5",
                                      -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.1.quantile_values.0.value", "66.6",
                                      -1, LM_VT_DOUBLE);
  log_msg_set_value_by_name_with_type(msg, ".otel.metric.data.summary.data_points.1.flags", "777", -1, LM_VT_INTEGER);

  Metric metric;
  cr_assert(formatter.format(msg, metric));
  log_msg_unref(msg);

  _metric_summary_tc_asserts(metric);

  /* Raw */
  msg = _create_log_msg_with_dummy_resource_and_scope();
  ProtobufParser::store_raw(msg, metric);

  Metric metric_from_raw;
  cr_assert(formatter.format(msg, metric_from_raw));
  log_msg_unref(msg);

  _metric_summary_tc_asserts(metric_from_raw);
}

static void
_span_tc_asserts(const Span &span)
{
  cr_assert_eq(span.trace_id().length(), 16);
  cr_assert_eq(memcmp(span.trace_id().c_str(), "\0\1\2\3\4\5\6\7\0\1\2\3\4\5\6\7", 16), 0);
  cr_assert_eq(span.span_id().length(), 8);
  cr_assert_eq(memcmp(span.span_id().c_str(), "\0\1\2\3\4\5\6\7", 8), 0);
  cr_assert_str_eq(span.trace_state().c_str(), "dummy_trace_state");
  cr_assert_eq(span.parent_span_id().length(), 8);
  cr_assert_eq(memcmp(span.parent_span_id().c_str(), "\7\6\5\4\3\2\1\0", 8), 0);
  cr_assert_str_eq(span.name().c_str(), "dummy_name");
  cr_assert_eq(span.kind(), Span_SpanKind::Span_SpanKind_SPAN_KIND_PRODUCER);
  cr_assert_eq(span.start_time_unix_nano(), 111);
  cr_assert_eq(span.end_time_unix_nano(), 222);
  cr_assert_eq(span.attributes_size(), 1);
  cr_assert_str_eq(span.attributes().at(0).key().c_str(), "attr_0");
  cr_assert_str_eq(span.attributes().at(0).value().string_value().c_str(), "val_0");
  cr_assert_eq(span.dropped_attributes_count(), 333);
  cr_assert_eq(span.events_size(), 2);

  const Span_Event &event_0 = span.events().at(0);
  cr_assert_eq(event_0.time_unix_nano(), 444);
  cr_assert_str_eq(event_0.name().c_str(), "event_0");
  cr_assert_eq(event_0.attributes_size(), 1);
  cr_assert_str_eq(event_0.attributes().at(0).key().c_str(), "e_attr_0");
  cr_assert_str_eq(event_0.attributes().at(0).value().string_value().c_str(), "e_val_0");
  cr_assert_eq(event_0.dropped_attributes_count(), 555);

  const Span_Event &event_1 = span.events().at(1);
  cr_assert_eq(event_1.time_unix_nano(), 666);
  cr_assert_str_eq(event_1.name().c_str(), "event_1");
  cr_assert_eq(event_1.attributes_size(), 1);
  cr_assert_str_eq(event_1.attributes().at(0).key().c_str(), "e_attr_1");
  cr_assert_str_eq(event_1.attributes().at(0).value().string_value().c_str(), "e_val_1");
  cr_assert_eq(event_1.dropped_attributes_count(), 777);

  cr_assert_eq(span.dropped_events_count(), 888);
  cr_assert_eq(span.links_size(), 2);

  const Span_Link &link_0 = span.links().at(0);
  cr_assert_eq(link_0.trace_id().length(), 16);
  cr_assert_eq(memcmp(link_0.trace_id().c_str(), "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16), 0);
  cr_assert_eq(link_0.span_id().length(), 8);
  cr_assert_eq(memcmp(link_0.span_id().c_str(), "\1\1\1\1\1\1\1\1", 8), 0);
  cr_assert_str_eq(link_0.trace_state().c_str(), "l_trace_state_0");
  cr_assert_eq(link_0.attributes_size(), 1);
  cr_assert_str_eq(link_0.attributes().at(0).key().c_str(), "l_attr_0");
  cr_assert_str_eq(link_0.attributes().at(0).value().string_value().c_str(), "l_val_0");
  cr_assert_eq(link_0.dropped_attributes_count(), 999);

  const Span_Link &link_1 = span.links().at(1);
  cr_assert_eq(link_1.trace_id().length(), 16);
  cr_assert_eq(memcmp(link_1.trace_id().c_str(), "\2\2\2\2\2\2\2\2\2\2\2\2\2\2\2\2", 16), 0);
  cr_assert_eq(link_1.span_id().length(), 8);
  cr_assert_eq(memcmp(link_1.span_id().c_str(), "\3\3\3\3\3\3\3\3", 8), 0);
  cr_assert_str_eq(link_1.trace_state().c_str(), "l_trace_state_1");
  cr_assert_eq(link_1.attributes_size(), 1);
  cr_assert_str_eq(link_1.attributes().at(0).key().c_str(), "l_attr_1");
  cr_assert_str_eq(link_1.attributes().at(0).value().string_value().c_str(), "l_val_1");
  cr_assert_eq(link_1.dropped_attributes_count(), 1111);

  cr_assert_str_eq(span.status().message().c_str(), "dummy_status_message");
  cr_assert_eq(span.status().code(), Status_StatusCode::Status_StatusCode_STATUS_CODE_ERROR);
}

Test(otel_protobuf_formatter, span)
{
  ProtobufFormatter formatter(configuration);
  LogMessage *msg = _create_log_msg_with_dummy_resource_and_scope();

  log_msg_set_value_by_name_with_type(msg, ".otel.span.trace_id", "\0\1\2\3\4\5\6\7\0\1\2\3\4\5\6\7", 16, LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.span_id", "\0\1\2\3\4\5\6\7", 8, LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.trace_state", "dummy_trace_state", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.parent_span_id", "\7\6\5\4\3\2\1\0", 8, LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.name", "dummy_name", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.kind", "4", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.start_time_unix_nano", "111", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.end_time_unix_nano", "222", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.attributes.attr_0", "val_0", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.dropped_attributes_count", "333", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.events.0.time_unix_nano", "444", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.events.0.name", "event_0", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.events.0.attributes.e_attr_0", "e_val_0", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.events.0.dropped_attributes_count", "555", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.events.1.time_unix_nano", "666", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.events.1.name", "event_1", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.events.1.attributes.e_attr_1", "e_val_1", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.events.1.dropped_attributes_count", "777", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.dropped_events_count", "888", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.links.0.trace_id", "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16,
                                      LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.links.0.span_id", "\1\1\1\1\1\1\1\1", 8, LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.links.0.trace_state", "l_trace_state_0", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.links.0.attributes.l_attr_0", "l_val_0", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.links.0.dropped_attributes_count", "999", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.links.1.trace_id", "\2\2\2\2\2\2\2\2\2\2\2\2\2\2\2\2", 16,
                                      LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.links.1.span_id", "\3\3\3\3\3\3\3\3", 8, LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.links.1.trace_state", "l_trace_state_1", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.links.1.attributes.l_attr_1", "l_val_1", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.links.1.dropped_attributes_count", "1111", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.status.message", "dummy_status_message", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.span.status.code", "2", -1, LM_VT_INTEGER);

  Span span;
  cr_assert(formatter.format(msg, span));
  log_msg_unref(msg);

  _span_tc_asserts(span);

  /* Raw */
  msg = _create_log_msg_with_dummy_resource_and_scope();
  ProtobufParser::store_raw(msg, span);

  Span span_from_raw;
  cr_assert(formatter.format(msg, span_from_raw));
  log_msg_unref(msg);

  _span_tc_asserts(span_from_raw);
}

void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
  otel_logmsg_handles_global_init();
}

void
teardown(void)
{
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(otel_protobuf_formatter, .init = setup, .fini = teardown);
