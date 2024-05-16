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

#include "opentelemetry/proto/logs/v1/logs.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/trace/v1/trace.pb.h"

#include "otel-protobuf-parser.hpp"
#include "otel-logmsg-handles.hpp"

#include "compat/cpp-start.h"
#include "apphook.h"
#include "compat/cpp-end.h"

#include <criterion/criterion.h>

using namespace syslogng::grpc::otel;
using namespace opentelemetry::proto::resource::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::logs::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::trace::v1;


static LogMessage *
_create_dummy_log_msg()
{
  LogMessage *msg = log_msg_new_empty();

  grpc::string peer = "ipv4:127.0.0.5:36372";
  Resource resource;
  std::string resource_schema_url = "dummy_resource_schema_url";
  InstrumentationScope scope;
  std::string scope_schema_url = "dummy_scope_schema_url";

  ProtobufParser::store_raw_metadata(msg, peer, resource, resource_schema_url, scope, scope_schema_url);

  return msg;
}

static void
_assert_log_msg_double_value(LogMessage *msg, const gchar *name, double expected_value)
{
  gssize length;
  LogMessageValueType type;
  const char *str_value = log_msg_get_value_by_name_with_type(msg, name, &length, &type);
  double value = std::atof(str_value);

  cr_assert_eq(type, LM_VT_DOUBLE);
  cr_assert_float_eq(value, expected_value, std::numeric_limits<double>::epsilon());
}

static void
_assert_log_msg_value(LogMessage *msg, const gchar *name, const gchar *expected_value, gssize expected_length,
                      LogMessageValueType expected_type)
{
  if (expected_length == -1)
    expected_length = strlen(expected_value);

  gssize length;
  LogMessageValueType type;
  const char *value = log_msg_get_value_by_name_with_type(msg, name, &length, &type);

  cr_assert_eq(length, expected_length);
  cr_assert_eq(memcmp(value, expected_value, length), 0);
  cr_assert_eq(type, expected_type);
}

/* This testcase also tests the the handling of different types of KeyValues. */
Test(otel_protobuf_parser, metadata)
{
  grpc::string peer = "ipv6:[::1]:36372";

  Resource resource;

  KeyValue *null_attr = resource.add_attributes();
  null_attr->set_key("null_key");

  KeyValue *string_attr = resource.add_attributes();
  string_attr->set_key("string_key");
  string_attr->mutable_value()->set_string_value("string_attribute");

  KeyValue *bool_attr = resource.add_attributes();
  bool_attr->set_key("bool_key");
  bool_attr->mutable_value()->set_bool_value(true);

  KeyValue *int_attr = resource.add_attributes();
  int_attr->set_key("int_key");
  int_attr->mutable_value()->set_int_value(42);

  KeyValue *double_attr = resource.add_attributes();
  double_attr->set_key("double_key");
  double_attr->mutable_value()->set_double_value(13.37);

  std::string resource_schema_url = "my_resource_schema_url";

  InstrumentationScope scope;
  scope.set_name("my_scope_name");
  scope.set_version("v1.2.3");

  KeyValue *list_attr = scope.add_attributes();
  list_attr->set_key("list_key");
  ArrayValue *list_value = list_attr->mutable_value()->mutable_array_value();
  list_value->add_values()->set_string_value("inner_list_attribute_value_1,");
  list_value->add_values()->set_string_value("inner_list_attribute_value_2");

  KeyValue *kvlist_attr = scope.add_attributes();
  kvlist_attr->set_key("kvlist_key");
  KeyValueList *key_value_list =  kvlist_attr->mutable_value()->mutable_kvlist_value();
  KeyValue *inner_kvlist_attr_1 = key_value_list->add_values();
  inner_kvlist_attr_1->set_key("inner_kvlist_attribute_key_1");
  inner_kvlist_attr_1->mutable_value()->set_string_value("inner_kvlist_attribute_value_1");
  KeyValue *inner_kvlist_attr_2 = key_value_list->add_values();
  inner_kvlist_attr_2->set_key("inner_kvlist_attribute_key_2");
  inner_kvlist_attr_2->mutable_value()->set_string_value("inner_kvlist_attribute_value_2");

  KeyValue *protobuf_list_attr = scope.add_attributes();
  protobuf_list_attr->set_key("protobuf_list_key");
  ArrayValue *protobuf_list_value = protobuf_list_attr->mutable_value()->mutable_array_value();
  protobuf_list_value->add_values()->set_string_value("inner_protobuf_list_attribute_value_1,");
  protobuf_list_value->add_values()->set_int_value(42);

  KeyValue *bytes_attr = scope.add_attributes();
  bytes_attr->set_key("bytes_key");
  bytes_attr->mutable_value()->set_bytes_value({0, 1, 2, 3, 4, 5, 6, 7});

  KeyValue *hostname = resource.add_attributes();
  hostname->set_key("host.name");
  hostname->mutable_value()->set_string_value("myhost");

  std::string scope_schema_url = "my_scope_schema_url";

  LogMessage *msg = log_msg_new_empty();
  ProtobufParser::store_raw_metadata(msg, peer, resource, resource_schema_url, scope, scope_schema_url);
  ProtobufParser::store_raw(msg, LogRecord());
  ProtobufParser().process(msg);

  cr_assert(msg->saddr != NULL);
  _assert_log_msg_value(msg, "SOURCEIP", "::1", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, "HOST", "myhost", -1, LM_VT_STRING);

  _assert_log_msg_value(msg, ".otel.resource.attributes.null_key", "", -1, LM_VT_NULL);
  _assert_log_msg_value(msg, ".otel.resource.attributes.string_key", "string_attribute", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.resource.attributes.bool_key", "true", -1, LM_VT_BOOLEAN);
  _assert_log_msg_value(msg, ".otel.resource.attributes.int_key", "42", -1, LM_VT_INTEGER);
  _assert_log_msg_double_value(msg, ".otel.resource.attributes.double_key", 13.37);

  _assert_log_msg_value(msg, ".otel.resource.schema_url", "my_resource_schema_url", -1, LM_VT_STRING);

  _assert_log_msg_value(msg, ".otel.scope.name", "my_scope_name", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.scope.version", "v1.2.3", -1, LM_VT_STRING);

  std::string serialized_kvlist_attr = kvlist_attr->value().SerializeAsString();
  _assert_log_msg_value(msg, ".otel.scope.attributes.kvlist_key", serialized_kvlist_attr.c_str(),
                        serialized_kvlist_attr.length(), LM_VT_PROTOBUF);
  _assert_log_msg_value(msg, ".otel.scope.attributes.list_key",
                        "\"inner_list_attribute_value_1,\",inner_list_attribute_value_2", -1, LM_VT_LIST);

  std::string serialized_protobuf_list_attr = protobuf_list_attr->value().SerializeAsString();
  _assert_log_msg_value(msg, ".otel.scope.attributes.protobuf_list_key", serialized_protobuf_list_attr.c_str(),
                        serialized_protobuf_list_attr.length(), LM_VT_PROTOBUF);

  std::string serialized_bytes_attr = bytes_attr->SerializeAsString();
  _assert_log_msg_value(msg, ".otel.scope.attributes.bytes_key", "\0\1\2\3\4\5\6\7", 8, LM_VT_BYTES);
  _assert_log_msg_value(msg, ".otel.scope.schema_url", "my_scope_schema_url", -1, LM_VT_STRING);

  log_msg_unref(msg);
}

Test(otel_protobuf_parser, log_record)
{
  LogMessage *msg = _create_dummy_log_msg();
  LogRecord log_record;

  log_record.set_time_unix_nano(111000222000);
  log_record.set_observed_time_unix_nano(333000444000);
  log_record.set_severity_number(SeverityNumber::SEVERITY_NUMBER_ERROR);
  log_record.set_severity_text("my_error_text");
  log_record.mutable_body()->set_string_value("string_body");

  KeyValue *attr_1 = log_record.add_attributes();
  attr_1->set_key("key_1");
  attr_1->mutable_value()->set_string_value("attribute_1");

  KeyValue *attr_2 = log_record.add_attributes();
  attr_2->set_key("key_2");
  attr_2->mutable_value()->set_string_value("attribute_2");

  log_record.set_dropped_attributes_count(11);
  log_record.set_flags(22);
  log_record.set_trace_id({0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7});
  log_record.set_span_id({0, 1, 2, 3, 4, 5, 6, 7});

  ProtobufParser::store_raw(msg, log_record);
  cr_assert(ProtobufParser().process(msg));

  cr_assert(msg->saddr != NULL);

  _assert_log_msg_value(msg, "SOURCEIP", "127.0.0.5", -1, LM_VT_STRING);

  _assert_log_msg_value(msg, ".otel.type", "log", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.log.time_unix_nano", "111000222000", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.log.observed_time_unix_nano", "333000444000", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.log.severity_number", "17", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.log.severity_text", "my_error_text", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.log.body", "string_body", -1, LM_VT_STRING);

  _assert_log_msg_value(msg, "MESSAGE", "string_body", -1, LM_VT_STRING);
  cr_assert_eq(LOG_PRI(msg->pri), LOG_ERR);
  cr_assert_eq(msg->timestamps[LM_TS_STAMP].ut_sec, 111);
  cr_assert_eq(msg->timestamps[LM_TS_STAMP].ut_usec, 222);
  cr_assert_eq(msg->timestamps[LM_TS_RECVD].ut_sec, 333);
  cr_assert_eq(msg->timestamps[LM_TS_RECVD].ut_usec, 444);

  _assert_log_msg_value(msg, ".otel.log.attributes.key_1", "attribute_1", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.log.attributes.key_2", "attribute_2", -1, LM_VT_STRING);

  _assert_log_msg_value(msg, ".otel.log.dropped_attributes_count", "11", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.log.flags", "22", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.log.trace_id", "\0\1\2\3\4\5\6\7\0\1\2\3\4\5\6\7", 16, LM_VT_BYTES);
  _assert_log_msg_value(msg, ".otel.log.span_id", "\0\1\2\3\4\5\6\7", 8, LM_VT_BYTES);

  log_msg_unref(msg);
}

Test(otel_protobuf_parser, log_record_body_types)
{
  LogMessage *msg;
  LogRecord log_record;
  ProtobufParser parser;

  msg = _create_dummy_log_msg();
  ProtobufParser::store_raw(msg, log_record);
  parser.process(msg);
  _assert_log_msg_value(msg, ".otel.log.body", "", -1, LM_VT_NULL);
  log_msg_unref(msg);

  msg = _create_dummy_log_msg();
  log_record.mutable_body()->set_string_value("string");
  ProtobufParser::store_raw(msg, log_record);
  parser.process(msg);
  _assert_log_msg_value(msg, ".otel.log.body", "string", -1, LM_VT_STRING);
  log_msg_unref(msg);

  msg = _create_dummy_log_msg();
  log_record.mutable_body()->set_bool_value(true);
  ProtobufParser::store_raw(msg, log_record);
  parser.process(msg);
  _assert_log_msg_value(msg, ".otel.log.body", "true", -1, LM_VT_BOOLEAN);
  log_msg_unref(msg);

  msg = _create_dummy_log_msg();
  log_record.mutable_body()->set_int_value(42);
  ProtobufParser::store_raw(msg, log_record);
  parser.process(msg);
  _assert_log_msg_value(msg, ".otel.log.body", "42", -1, LM_VT_INTEGER);
  log_msg_unref(msg);

  msg = _create_dummy_log_msg();
  log_record.mutable_body()->set_double_value(13.37);
  ProtobufParser::store_raw(msg, log_record);
  parser.process(msg);
  _assert_log_msg_double_value(msg, ".otel.log.body", 13.37);
  log_msg_unref(msg);

  msg = _create_dummy_log_msg();
  ArrayValue *list_value = log_record.mutable_body()->mutable_array_value();
  list_value->add_values()->set_string_value("list_value_1,");
  list_value->add_values()->set_string_value("list_value_2");
  ProtobufParser::store_raw(msg, log_record);
  parser.process(msg);
  _assert_log_msg_value(msg, ".otel.log.body", "\"list_value_1,\",list_value_2", -1, LM_VT_LIST);
  log_msg_unref(msg);

  msg = _create_dummy_log_msg();
  KeyValueList *kvlist_value =  log_record.mutable_body()->mutable_kvlist_value();
  KeyValue *inner_kvlist_kv_1 = kvlist_value->add_values();
  inner_kvlist_kv_1->set_key("inner_kvlist_key_1");
  inner_kvlist_kv_1->mutable_value()->set_string_value("inner_kvlist_value_1");
  KeyValue *inner_kvlist_kv_2 = kvlist_value->add_values();
  inner_kvlist_kv_2->set_key("inner_kvlist_key_2");
  inner_kvlist_kv_2->mutable_value()->set_string_value("inner_kvlist_value_2");
  ProtobufParser::store_raw(msg, log_record);
  parser.process(msg);
  std::string serialized_kvlist_value = log_record.body().SerializeAsString();
  _assert_log_msg_value(msg, ".otel.log.body", serialized_kvlist_value.c_str(), serialized_kvlist_value.length(),
                        LM_VT_PROTOBUF);

  msg = _create_dummy_log_msg();
  ArrayValue *protobuf_list_value = log_record.mutable_body()->mutable_array_value();
  protobuf_list_value->add_values()->set_string_value("protobuf_list_value_1");
  protobuf_list_value->add_values()->set_int_value(42);
  ProtobufParser::store_raw(msg, log_record);
  parser.process(msg);
  std::string serialized_protobuf_list_value = log_record.body().SerializeAsString();
  _assert_log_msg_value(msg, ".otel.log.body",
                        serialized_protobuf_list_value.c_str(), serialized_protobuf_list_value.length(),
                        LM_VT_PROTOBUF);
  log_msg_unref(msg);

  msg = _create_dummy_log_msg();
  log_record.mutable_body()->set_bytes_value({0, 1, 2});
  ProtobufParser::store_raw(msg, log_record);
  parser.process(msg);
  _assert_log_msg_value(msg, ".otel.log.body", "\0\1\2", 3, LM_VT_BYTES);
  log_msg_unref(msg);
}

Test(otel_protobuf_parser, metric_common)
{
  LogMessage *msg = _create_dummy_log_msg();
  Metric metric;

  metric.set_name("my_metric_name");
  metric.set_description("My metric description");
  metric.set_unit("my_unit");

  ProtobufParser::store_raw(msg, metric);
  cr_assert(ProtobufParser().process(msg));

  _assert_log_msg_value(msg, ".otel.type", "metric", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.name", "my_metric_name", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.description", "My metric description", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.unit", "my_unit", -1, LM_VT_STRING);

  log_msg_unref(msg);
}

/* This testcase also tests the the handling of Exemplars. */
Test(otel_protobuf_parser, metric_gauge)
{
  LogMessage *msg = _create_dummy_log_msg();
  Metric metric;

  Gauge *gauge = metric.mutable_gauge();

  NumberDataPoint *data_point_0 = gauge->add_data_points();
  KeyValue *attr_0_0 = data_point_0->add_attributes();
  attr_0_0->set_key("attr_0_0_key");
  attr_0_0->mutable_value()->set_string_value("attr_0_0_value");
  KeyValue *attr_0_1 = data_point_0->add_attributes();
  attr_0_1->set_key("attr_0_1_key");
  attr_0_1->mutable_value()->set_string_value("attr_0_1_value");
  data_point_0->set_start_time_unix_nano(123);
  data_point_0->set_time_unix_nano(456);
  data_point_0->set_as_double(13.37);
  Exemplar *exemplar_0_0 = data_point_0->add_exemplars();
  KeyValue *exemplar_attr_0_0_0 = exemplar_0_0->add_filtered_attributes();
  exemplar_attr_0_0_0->set_key("exemplar_attr_0_0_0_key");
  exemplar_attr_0_0_0->mutable_value()->set_string_value("exemplar_attr_0_0_0_value");
  KeyValue *exemplar_attr_0_0_1 = exemplar_0_0->add_filtered_attributes();
  exemplar_attr_0_0_1->set_key("exemplar_attr_0_0_1_key");
  exemplar_attr_0_0_1->mutable_value()->set_string_value("exemplar_attr_0_0_1_value");
  exemplar_0_0->set_time_unix_nano(111);
  exemplar_0_0->set_as_double(4.2);
  exemplar_0_0->set_span_id({0, 1, 2, 3, 4, 5, 6, 7});
  exemplar_0_0->set_trace_id({0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7});
  Exemplar *exemplar_0_1 = data_point_0->add_exemplars();
  exemplar_0_1->set_time_unix_nano(222);
  exemplar_0_1->set_as_int(42);
  data_point_0->set_flags(666);

  NumberDataPoint *data_point_1 = gauge->add_data_points();
  data_point_1->set_start_time_unix_nano(777);
  data_point_1->set_time_unix_nano(888);
  data_point_1->set_as_int(1337);
  data_point_1->set_flags(999);

  ProtobufParser::store_raw(msg, metric);
  cr_assert(ProtobufParser().process(msg));

  _assert_log_msg_value(msg, ".otel.metric.data.type", "gauge", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.0.attributes.attr_0_0_key", "attr_0_0_value", -1,
                        LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.0.attributes.attr_0_1_key", "attr_0_1_value", -1,
                        LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.0.start_time_unix_nano", "123", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.0.time_unix_nano", "456", -1, LM_VT_INTEGER);
  _assert_log_msg_double_value(msg, ".otel.metric.data.gauge.data_points.0.value", 13.37);
  _assert_log_msg_value(msg,
                        ".otel.metric.data.gauge.data_points.0.exemplars.0.filtered_attributes.exemplar_attr_0_0_0_key",
                        "exemplar_attr_0_0_0_value", -1, LM_VT_STRING);
  _assert_log_msg_value(msg,
                        ".otel.metric.data.gauge.data_points.0.exemplars.0.filtered_attributes.exemplar_attr_0_0_1_key",
                        "exemplar_attr_0_0_1_value", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.0.exemplars.0.time_unix_nano", "111", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_double_value(msg, ".otel.metric.data.gauge.data_points.0.exemplars.0.value", 4.2);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.0.exemplars.0.span_id", "\0\1\2\3\4\5\6\7", 8,
                        LM_VT_BYTES);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.0.exemplars.0.trace_id",
                        "\0\1\2\3\4\5\6\7\0\1\2\3\4\5\6\7", 16, LM_VT_BYTES);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.0.exemplars.1.time_unix_nano", "222", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.0.exemplars.1.value", "42", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.0.flags", "666", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.1.start_time_unix_nano", "777", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.1.time_unix_nano", "888", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.1.value", "1337", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.gauge.data_points.1.flags", "999", -1, LM_VT_INTEGER);

  log_msg_unref(msg);
}

Test(otel_protobuf_parser, metric_sum)
{
  LogMessage *msg = _create_dummy_log_msg();
  Metric metric;

  Sum *sum = metric.mutable_sum();

  /* Extensive NumberDataPoint tests are done in otel_protobuf_parser::metric_gauge() */
  NumberDataPoint *data_point = sum->add_data_points();
  data_point->set_start_time_unix_nano(777);
  data_point->set_time_unix_nano(888);
  data_point->set_as_int(1337);
  data_point->set_flags(999);

  sum->set_aggregation_temporality(AGGREGATION_TEMPORALITY_DELTA);
  sum->set_is_monotonic(true);

  ProtobufParser::store_raw(msg, metric);
  cr_assert(ProtobufParser().process(msg));

  _assert_log_msg_value(msg, ".otel.metric.data.type", "sum", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.sum.data_points.0.start_time_unix_nano", "777", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.sum.data_points.0.time_unix_nano", "888", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.sum.data_points.0.value", "1337", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.sum.data_points.0.flags", "999", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.sum.aggregation_temporality", "1", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.sum.is_monotonic", "true", -1, LM_VT_BOOLEAN);

  log_msg_unref(msg);
}

Test(otel_protobuf_parser, metric_histogram)
{
  LogMessage *msg = _create_dummy_log_msg();
  Metric metric;

  Histogram *histogram = metric.mutable_histogram();

  HistogramDataPoint *data_point_0 = histogram->add_data_points();
  KeyValue *attr_0 = data_point_0->add_attributes();
  attr_0->set_key("attr_0_key");
  attr_0->mutable_value()->set_string_value("attr_0_value");
  KeyValue *attr_1 = data_point_0->add_attributes();
  attr_1->set_key("attr_1_key");
  attr_1->mutable_value()->set_string_value("attr_1_value");
  data_point_0->set_start_time_unix_nano(777);
  data_point_0->set_time_unix_nano(888);
  data_point_0->set_count(2);
  data_point_0->set_sum(42.0);
  data_point_0->add_bucket_counts(20);
  data_point_0->add_bucket_counts(22);
  data_point_0->add_explicit_bounds(1);
  data_point_0->add_explicit_bounds(2);
  Exemplar *exemplar = data_point_0->add_exemplars();
  exemplar->set_time_unix_nano(222);
  exemplar->set_as_int(42);
  data_point_0->set_flags(666);
  data_point_0->set_min(20.5);
  data_point_0->set_max(21.5);

  HistogramDataPoint *data_point_1 = histogram->add_data_points();
  data_point_1->set_start_time_unix_nano(555);
  data_point_1->set_time_unix_nano(666);
  data_point_1->set_count(1);
  data_point_1->set_sum(1.1);
  data_point_1->add_bucket_counts(1);
  data_point_1->add_explicit_bounds(1);
  data_point_1->set_flags(777);
  data_point_1->set_min(1.1);
  data_point_1->set_max(1.2);

  ProtobufParser::store_raw(msg, metric);
  cr_assert(ProtobufParser().process(msg));

  _assert_log_msg_value(msg, ".otel.metric.data.type", "histogram", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.0.attributes.attr_0_key", "attr_0_value", -1,
                        LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.0.attributes.attr_1_key", "attr_1_value", -1,
                        LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.0.start_time_unix_nano", "777", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.0.time_unix_nano", "888", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.0.count", "2", -1, LM_VT_INTEGER);
  _assert_log_msg_double_value(msg, ".otel.metric.data.histogram.data_points.0.sum", 42);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.0.bucket_counts.0", "20", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.0.bucket_counts.1", "22", -1, LM_VT_INTEGER);
  _assert_log_msg_double_value(msg, ".otel.metric.data.histogram.data_points.0.explicit_bounds.0", 1);
  _assert_log_msg_double_value(msg, ".otel.metric.data.histogram.data_points.0.explicit_bounds.1", 2);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.0.exemplars.0.time_unix_nano", "222", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.0.exemplars.0.value", "42", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.0.flags", "666", -1, LM_VT_INTEGER);
  _assert_log_msg_double_value(msg, ".otel.metric.data.histogram.data_points.0.min", 20.5);
  _assert_log_msg_double_value(msg, ".otel.metric.data.histogram.data_points.0.max", 21.5);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.1.start_time_unix_nano", "555", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.1.time_unix_nano", "666", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.1.count", "1", -1, LM_VT_INTEGER);
  _assert_log_msg_double_value(msg, ".otel.metric.data.histogram.data_points.1.sum", 1.1);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.1.bucket_counts.0", "1", -1, LM_VT_INTEGER);
  _assert_log_msg_double_value(msg, ".otel.metric.data.histogram.data_points.1.explicit_bounds.0", 1);
  _assert_log_msg_value(msg, ".otel.metric.data.histogram.data_points.1.flags", "777", -1, LM_VT_INTEGER);
  _assert_log_msg_double_value(msg, ".otel.metric.data.histogram.data_points.1.min", 1.1);
  _assert_log_msg_double_value(msg, ".otel.metric.data.histogram.data_points.1.max", 1.2);

  log_msg_unref(msg);
}

Test(otel_protobuf_parser, metric_exponential_histogram)
{
  LogMessage *msg = _create_dummy_log_msg();
  Metric metric;

  ExponentialHistogram *exponential_histogram = metric.mutable_exponential_histogram();

  ExponentialHistogramDataPoint *data_point_0 = exponential_histogram->add_data_points();
  KeyValue *attr_0_0 = data_point_0->add_attributes();
  attr_0_0->set_key("attr_0_0_key");
  attr_0_0->mutable_value()->set_string_value("attr_0_0_value");
  KeyValue *attr_0_1 = data_point_0->add_attributes();
  attr_0_1->set_key("attr_0_1_key");
  attr_0_1->mutable_value()->set_string_value("attr_0_1_value");
  data_point_0->set_start_time_unix_nano(123);
  data_point_0->set_time_unix_nano(456);
  data_point_0->set_count(2);
  data_point_0->set_sum(42.0);
  data_point_0->set_scale(3);
  data_point_0->set_zero_count(4);
  data_point_0->mutable_positive()->set_offset(12);
  data_point_0->mutable_positive()->add_bucket_counts(111);
  data_point_0->mutable_positive()->add_bucket_counts(222);
  data_point_0->mutable_negative()->set_offset(45);
  data_point_0->mutable_negative()->add_bucket_counts(333);
  data_point_0->mutable_negative()->add_bucket_counts(444);
  data_point_0->set_flags(555);
  Exemplar *exemplar = data_point_0->add_exemplars();
  exemplar->set_time_unix_nano(222);
  exemplar->set_as_int(42);
  data_point_0->set_min(111.0);
  data_point_0->set_max(444.0);
  data_point_0->set_zero_threshold(123.456789);

  ExponentialHistogramDataPoint *data_point_1 = exponential_histogram->add_data_points();
  data_point_1->set_start_time_unix_nano(111);
  data_point_1->set_time_unix_nano(222);
  data_point_1->set_count(3);
  data_point_1->set_scale(4);
  data_point_1->set_zero_count(5);
  data_point_1->mutable_positive()->set_offset(11);
  data_point_1->mutable_positive()->add_bucket_counts(222);
  data_point_1->mutable_negative()->set_offset(33);
  data_point_1->mutable_negative()->add_bucket_counts(444);
  data_point_1->set_flags(666);

  exponential_histogram->set_aggregation_temporality(AGGREGATION_TEMPORALITY_CUMULATIVE);

  ProtobufParser::store_raw(msg, metric);
  cr_assert(ProtobufParser().process(msg));

  _assert_log_msg_value(msg, ".otel.metric.data.type", "exponential_histogram", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.attributes.attr_0_0_key",
                        "attr_0_0_value", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.attributes.attr_0_1_key",
                        "attr_0_1_value", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.start_time_unix_nano", "123", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.time_unix_nano", "456", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.count", "2", -1, LM_VT_INTEGER);
  _assert_log_msg_double_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.sum", 42);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.scale", "3", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.zero_count", "4", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.positive.offset", "12", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.positive.bucket_counts.0", "111",
                        -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.positive.bucket_counts.1", "222",
                        -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.negative.offset", "45", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.negative.bucket_counts.0", "333",
                        -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.negative.bucket_counts.1", "444",
                        -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.flags", "555", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.exemplars.0.time_unix_nano", "222",
                        -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.exemplars.0.value", "42", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_double_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.min", 111);
  _assert_log_msg_double_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.max", 444);
  _assert_log_msg_double_value(msg, ".otel.metric.data.exponential_histogram.data_points.0.zero_threshold", 123.456789);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.1.start_time_unix_nano", "111", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.1.time_unix_nano", "222", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.1.count", "3", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.1.scale", "4", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.1.zero_count", "5", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.1.positive.offset", "11", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.1.positive.bucket_counts.0", "222",
                        -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.1.negative.offset", "33", -1,
                        LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.1.negative.bucket_counts.0", "444",
                        -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.data_points.1.flags", "666", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.exponential_histogram.aggregation_temporality", "2", -1, LM_VT_INTEGER);

  log_msg_unref(msg);
}

Test(otel_protobuf_parser, metric_summary)
{
  LogMessage *msg = _create_dummy_log_msg();
  Metric metric;

  Summary *summary = metric.mutable_summary();

  SummaryDataPoint *data_point_0 = summary->add_data_points();
  KeyValue *attr_0_0 = data_point_0->add_attributes();
  attr_0_0->set_key("attr_0_0_key");
  attr_0_0->mutable_value()->set_string_value("attr_0_0_value");
  KeyValue *attr_0_1 = data_point_0->add_attributes();
  attr_0_1->set_key("attr_0_1_key");
  attr_0_1->mutable_value()->set_string_value("attr_0_1_value");
  data_point_0->set_start_time_unix_nano(123);
  data_point_0->set_time_unix_nano(456);
  data_point_0->set_count(2);
  data_point_0->set_sum(42.0);
  SummaryDataPoint::ValueAtQuantile *value_at_quantile_0_0 = data_point_0->add_quantile_values();
  value_at_quantile_0_0->set_quantile(0.1);
  value_at_quantile_0_0->set_value(0.2);
  SummaryDataPoint::ValueAtQuantile *value_at_quantile_0_1 = data_point_0->add_quantile_values();
  value_at_quantile_0_1->set_quantile(0.3);
  value_at_quantile_0_1->set_value(0.4);
  data_point_0->set_flags(789);

  SummaryDataPoint *data_point_1 = summary->add_data_points();
  data_point_1->set_start_time_unix_nano(111);
  data_point_1->set_time_unix_nano(222);
  data_point_1->set_count(1);
  data_point_1->set_sum(13.37);
  SummaryDataPoint::ValueAtQuantile *value_at_quantile_1_0 = data_point_1->add_quantile_values();
  value_at_quantile_1_0->set_quantile(0.6);
  value_at_quantile_1_0->set_value(0.7);
  data_point_1->set_flags(444);

  ProtobufParser::store_raw(msg, metric);
  cr_assert(ProtobufParser().process(msg));

  _assert_log_msg_value(msg, ".otel.metric.data.type", "summary", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.summary.data_points.0.attributes.attr_0_0_key", "attr_0_0_value", -1,
                        LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.summary.data_points.0.attributes.attr_0_1_key", "attr_0_1_value", -1,
                        LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.metric.data.summary.data_points.0.start_time_unix_nano", "123", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.summary.data_points.0.time_unix_nano", "456", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.summary.data_points.0.count", "2", -1, LM_VT_INTEGER);
  _assert_log_msg_double_value(msg, ".otel.metric.data.summary.data_points.0.sum", 42);
  _assert_log_msg_double_value(msg, ".otel.metric.data.summary.data_points.0.quantile_values.0.quantile", 0.1);
  _assert_log_msg_double_value(msg, ".otel.metric.data.summary.data_points.0.quantile_values.0.value", 0.2);
  _assert_log_msg_double_value(msg, ".otel.metric.data.summary.data_points.0.quantile_values.1.quantile", 0.3);
  _assert_log_msg_double_value(msg, ".otel.metric.data.summary.data_points.0.quantile_values.1.value", 0.4);
  _assert_log_msg_value(msg, ".otel.metric.data.summary.data_points.0.flags", "789", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.summary.data_points.1.start_time_unix_nano", "111", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.summary.data_points.1.time_unix_nano", "222", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.metric.data.summary.data_points.1.count", "1", -1, LM_VT_INTEGER);
  _assert_log_msg_double_value(msg, ".otel.metric.data.summary.data_points.1.sum", 13.37);
  _assert_log_msg_double_value(msg, ".otel.metric.data.summary.data_points.1.quantile_values.0.quantile", 0.6);
  _assert_log_msg_double_value(msg, ".otel.metric.data.summary.data_points.1.quantile_values.0.value", 0.7);
  _assert_log_msg_value(msg, ".otel.metric.data.summary.data_points.1.flags", "444", -1, LM_VT_INTEGER);

  log_msg_unref(msg);
}

Test(otel_protobuf_parser, span)
{
  LogMessage *msg = _create_dummy_log_msg();
  Span span;

  span.set_trace_id({0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7});
  span.set_span_id({0, 1, 2, 3, 4, 5, 6, 7});
  span.set_trace_state("my_trace_state");
  span.set_parent_span_id({7, 6, 5, 4, 3, 2, 1, 0});
  span.set_name("my_name");
  span.set_kind(Span::SpanKind::Span_SpanKind_SPAN_KIND_INTERNAL);
  span.set_start_time_unix_nano(123);
  span.set_end_time_unix_nano(456);
  KeyValue *span_attr_0 = span.add_attributes();
  span_attr_0->set_key("span_attr_0_key");
  span_attr_0->mutable_value()->set_string_value("span_attr_0_value");
  KeyValue *span_attr_1 = span.add_attributes();
  span_attr_1->set_key("span_attr_1_key");
  span_attr_1->mutable_value()->set_string_value("span_attr_1_value");
  span.set_dropped_attributes_count(1);

  Span::Event *event_0 = span.add_events();
  event_0->set_time_unix_nano(111);
  event_0->set_name("event_0");
  KeyValue *event_0_attr_0 = event_0->add_attributes();
  event_0_attr_0->set_key("event_0_attr_0_key");
  event_0_attr_0->mutable_value()->set_string_value("event_0_attr_0_value");
  KeyValue *event_0_attr_1 = event_0->add_attributes();
  event_0_attr_1->set_key("event_0_attr_1_key");
  event_0_attr_1->mutable_value()->set_string_value("event_0_attr_1_value");
  event_0->set_dropped_attributes_count(2);

  Span::Event *event_1 = span.add_events();
  event_1->set_time_unix_nano(222);
  event_1->set_name("event_1");
  event_1->set_dropped_attributes_count(3);

  span.set_dropped_events_count(4);

  Span::Link *link_0 = span.add_links();
  link_0->set_trace_id({0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1});
  link_0->set_span_id({0, 2, 0, 2, 0, 2, 0, 2});
  link_0->set_trace_state("link_0_trace_state");
  KeyValue *link_0_attr_0 = link_0->add_attributes();
  link_0_attr_0->set_key("link_0_attr_0_key");
  link_0_attr_0->mutable_value()->set_string_value("link_0_attr_0_value");
  KeyValue *link_0_attr_1 = link_0->add_attributes();
  link_0_attr_1->set_key("link_0_attr_1_key");
  link_0_attr_1->mutable_value()->set_string_value("link_0_attr_1_value");
  link_0->set_dropped_attributes_count(5);

  Span::Link *link_1 = span.add_links();
  link_1->set_trace_id({0, 3, 0, 3, 0, 3, 0, 3, 0, 3, 0, 3, 0, 3, 0, 3});
  link_1->set_span_id({0, 4, 0, 4, 0, 4, 0, 4});
  link_1->set_trace_state("link_1_trace_state");
  link_1->set_dropped_attributes_count(6);

  span.set_dropped_links_count(7);
  span.mutable_status()->set_message("my_status");
  span.mutable_status()->set_code(Status::StatusCode::Status_StatusCode_STATUS_CODE_ERROR);

  ProtobufParser::store_raw(msg, span);
  cr_assert(ProtobufParser().process(msg));

  _assert_log_msg_value(msg, ".otel.type", "span", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.trace_id", "\0\1\2\3\4\5\6\7\0\1\2\3\4\5\6\7", 16, LM_VT_BYTES);
  _assert_log_msg_value(msg, ".otel.span.span_id", "\0\1\2\3\4\5\6\7", 8, LM_VT_BYTES);
  _assert_log_msg_value(msg, ".otel.span.trace_state", "my_trace_state", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.parent_span_id", "\7\6\5\4\3\2\1\0", 8, LM_VT_BYTES);
  _assert_log_msg_value(msg, ".otel.span.name", "my_name", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.kind", "1", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.span.start_time_unix_nano", "123", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.span.end_time_unix_nano", "456", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.span.attributes.span_attr_0_key", "span_attr_0_value", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.attributes.span_attr_1_key", "span_attr_1_value", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.dropped_attributes_count", "1", -1, LM_VT_INTEGER);

  _assert_log_msg_value(msg, ".otel.span.events.0.time_unix_nano", "111", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.span.events.0.name", "event_0", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.events.0.attributes.event_0_attr_0_key", "event_0_attr_0_value", -1,
                        LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.events.0.attributes.event_0_attr_1_key", "event_0_attr_1_value", -1,
                        LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.events.0.dropped_attributes_count", "2", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.span.events.1.time_unix_nano", "222", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.span.events.1.name", "event_1", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.events.1.dropped_attributes_count", "3", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.span.dropped_events_count", "4", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.span.links.0.trace_id", "\0\1\0\1\0\1\0\1\0\1\0\1\0\1\0\1", 16, LM_VT_BYTES);
  _assert_log_msg_value(msg, ".otel.span.links.0.span_id", "\0\2\0\2\0\2\0\2", 8, LM_VT_BYTES);
  _assert_log_msg_value(msg, ".otel.span.links.0.trace_state", "link_0_trace_state", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.links.0.attributes.link_0_attr_0_key", "link_0_attr_0_value", -1,
                        LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.links.0.attributes.link_0_attr_1_key", "link_0_attr_1_value", -1,
                        LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.links.0.dropped_attributes_count", "5", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.span.links.1.trace_id", "\0\3\0\3\0\3\0\3\0\3\0\3\0\3\0\3", 16, LM_VT_BYTES);
  _assert_log_msg_value(msg, ".otel.span.links.1.span_id", "\0\4\0\4\0\4\0\4", 8, LM_VT_BYTES);
  _assert_log_msg_value(msg, ".otel.span.links.1.trace_state", "link_1_trace_state", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.links.1.dropped_attributes_count", "6", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.span.dropped_links_count", "7", -1, LM_VT_INTEGER);
  _assert_log_msg_value(msg, ".otel.span.status.message", "my_status", -1, LM_VT_STRING);
  _assert_log_msg_value(msg, ".otel.span.status.code", "2", -1, LM_VT_INTEGER);

  log_msg_unref(msg);
}

static void
_setup(void)
{
  app_startup();
  otel_logmsg_handles_global_init();
}

static void
_teardown(void)
{
  app_shutdown();
}

TestSuite(otel_protobuf_parser, .init = _setup, .fini = _teardown);
