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


#include "otel-protobuf-parser.hpp"
#include "otel-logmsg-handles.hpp"

#include "compat/cpp-start.h"
#include "logmsg/type-hinting.h"
#include "scanner/list-scanner/list-scanner.h"
#include "rewrite/rewrite-set-pri.h"
#include "str-repr/encode.h"
#include "scratch-buffers.h"
#include "compat/cpp-end.h"

#include <inttypes.h>

using namespace syslogng::grpc::otel;
using namespace google::protobuf;
using namespace opentelemetry::proto::resource::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::logs::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::trace::v1;

#define get_ProtobufParser(s) (((OtelProtobufParser *) s)->cpp)

struct OtelProtobufParser_
{
  LogParser super;
  syslogng::grpc::otel::ProtobufParser *cpp;
};

static const gchar *
_get_string_field(LogMessage *msg, NVHandle handle, gssize *len)
{
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_with_type(msg, handle, len, &type);

  if (type != LM_VT_STRING)
    {
      msg_error("OpenTelemetry: unexpected LogMessage type, while getting string field",
                evt_tag_msg_reference(msg),
                evt_tag_str("name", log_msg_get_value_name(handle, NULL)),
                evt_tag_str("type", log_msg_value_type_to_str(type)));
      return nullptr;
    }

  return value;
}

static const gchar *
_get_protobuf_field(LogMessage *msg, NVHandle handle, gssize *len)
{
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_with_type(msg, handle, len, &type);

  if (type != LM_VT_PROTOBUF)
    {
      msg_error("OpenTelemetry: unexpected LogMessage type, while getting protobuf field",
                evt_tag_msg_reference(msg),
                evt_tag_str("name", log_msg_get_value_name(handle, NULL)),
                evt_tag_str("type", log_msg_value_type_to_str(type)));
      return nullptr;
    }

  return value;
}

static void
_set_value(LogMessage *msg, const char *key, const char *value, LogMessageValueType type)
{
  log_msg_set_value_by_name_with_type(msg, key, value, -1, type);
}

static void
_set_value(LogMessage *msg, NVHandle handle, const std::string &value, LogMessageValueType type)
{
  log_msg_set_value_with_type(msg, handle, value.c_str(), value.length(), type);
}

static void
_set_value_with_prefix(LogMessage *msg, std::string &key_buffer, size_t key_prefix_length, const char *key,
                       const std::string &value, LogMessageValueType type)
{
  key_buffer.resize(key_prefix_length);
  key_buffer.append(key);
  log_msg_set_value_by_name_with_type(msg, key_buffer.c_str(), value.c_str(), value.length(), type);
}

static const std::string &
_serialize_ArrayValue(const AnyValue &value, LogMessageValueType *type, std::string *buffer)
{
  bool is_all_strings = true;

  for (const AnyValue &element : value.array_value().values())
    {
      if (element.value_case() == AnyValue::kStringValue)
        continue;

      is_all_strings = false;
      break;
    }

  if (!is_all_strings)
    {
      *type = LM_VT_PROTOBUF;
      value.SerializePartialToString(buffer);
      return *buffer;
    }

  ScratchBuffersMarker marker;
  GString *scratch_buffer = scratch_buffers_alloc_and_mark(&marker);
  bool first = true;

  for (const AnyValue &element : value.array_value().values())
    {
      if (!first)
        g_string_append_c(scratch_buffer, ',');

      str_repr_encode_append(scratch_buffer, element.string_value().c_str(), -1, ",");
      first = false;
    }

  *type = LM_VT_LIST;
  buffer->assign(scratch_buffer->str, scratch_buffer->len);

  scratch_buffers_reclaim_marked(marker);
  return *buffer;
}

static const std::string &
_serialize_AnyValue(const AnyValue &value, LogMessageValueType *type, std::string *buffer)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];

  switch (value.value_case())
    {
    case AnyValue::kArrayValue:
      return _serialize_ArrayValue(value, type, buffer);
    case AnyValue::kKvlistValue:
      *type = LM_VT_PROTOBUF;
      value.SerializePartialToString(buffer);
      return *buffer;
    case AnyValue::kBytesValue:
      *type = LM_VT_BYTES;
      return value.bytes_value();
    case AnyValue::kBoolValue:
      *type = LM_VT_BOOLEAN;
      *buffer = value.bool_value() ? "true" : "false";
      return *buffer;
    case AnyValue::kDoubleValue:
      *type = LM_VT_DOUBLE;
      g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), value.double_value());
      *buffer = number_buf;
      return *buffer;
    case AnyValue::kIntValue:
      *type = LM_VT_INTEGER;
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIi64, value.int_value());
      *buffer = number_buf;
      return *buffer;
    case AnyValue::kStringValue:
      *type = LM_VT_STRING;
      return value.string_value();
    case AnyValue::VALUE_NOT_SET:
      *type = LM_VT_NULL;
      buffer->resize(0);
      return *buffer;
    default:
      msg_error("OpenTelemetry: unexpected AnyValue type", evt_tag_int("type", value.value_case()));
      buffer->resize(0);
      return *buffer;
    }
}

static void
_add_repeated_KeyValue_fields_with_prefix(LogMessage *msg, std::string &key_buffer, size_t key_prefix_length,
                                          const char *key, const RepeatedPtrField<KeyValue> &key_values)
{
  key_buffer.resize(key_prefix_length);
  key_buffer.append(key);
  key_buffer.append(".");
  size_t length_with_dot = key_buffer.length();
  std::string value_buffer;

  for (const KeyValue &kv : key_values)
    {
      /* <prefix>.<key>.<kv-key> */
      LogMessageValueType type;
      const std::string &value_serialized = _serialize_AnyValue(kv.value(), &type, &value_buffer);
      _set_value_with_prefix(msg, key_buffer, length_with_dot, kv.key().c_str(), value_serialized, type);
    }
}

static void
_add_repeated_KeyValue_fields(LogMessage *msg, const char *key, const RepeatedPtrField<KeyValue> &key_values)
{
  std::string key_buffer;
  _add_repeated_KeyValue_fields_with_prefix(msg, key_buffer, 0, key, key_values);
}

static void
_set_hostname_from_attributes(LogMessage *msg, const RepeatedPtrField<KeyValue> &key_values)
{
  for (const KeyValue &kv : key_values)
    {
      if (kv.key() == "host.name")
        {
          if (kv.value().value_case() != AnyValue::kStringValue)
            return;

          std::string hostname = kv.value().string_value();
          if (!hostname.empty())
            log_msg_set_value(msg, LM_V_HOST, hostname.c_str(), hostname.length());

          return;
        }
    }
}

static GSockAddr *
_extract_saddr(const grpc::string &peer)
{
  size_t first = peer.find_first_of(':');
  size_t last = peer.find_last_of(':');

  /* expected format:  ipv6:[::1]:32768 or ipv4:1.2.3.4:32768 */
  if (first != grpc::string::npos && last != grpc::string::npos)
    {
      const std::string ip_version = peer.substr(0, first);
      std::string host;
      int port = std::stoi(peer.substr(last + 1, grpc::string::npos), nullptr, 10);

      if (peer.at(first + 1) == '[')
        host = peer.substr(first + 2, last - first - 3);
      else
        host = peer.substr(first + 1, last - first - 1);

      if (ip_version.compare("ipv4") == 0)
        {
          return g_sockaddr_inet_new(host.c_str(), port);
        }
#if SYSLOG_NG_ENABLE_IPV6
      else if (ip_version.compare("ipv6") == 0)
        {
          return g_sockaddr_inet6_new(host.c_str(), port);
        }
#endif
    }

  return NULL;
}

static bool
_parse_metadata(LogMessage *msg, bool set_hostname)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];
  gssize len;
  const gchar *value;

  /* .otel.resource.<...> */
  value = _get_protobuf_field(msg, logmsg_handle::RAW_RESOURCE, &len);
  if (!value)
    return false;
  Resource resource;
  if (!resource.ParsePartialFromArray(value, len))
    {
      msg_error("OpenTelemetry: Failed to deserialize .otel_raw.resource",
                evt_tag_msg_reference(msg));
      return false;
    }

  /* .otel.resource.attributes */
  _add_repeated_KeyValue_fields(msg, ".otel.resource.attributes", resource.attributes());
  if (set_hostname)
    _set_hostname_from_attributes(msg, resource.attributes());

  /* .otel.resource.dropped_attributes_count */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, resource.dropped_attributes_count());
  _set_value(msg, logmsg_handle::RESOURCE_DROPPED_ATTRIBUTES_COUNT, number_buf, LM_VT_INTEGER);

  /* .otel.resource.schema_url */
  value = _get_string_field(msg, logmsg_handle::RAW_RESOURCE_SCHEMA_URL, &len);
  if (!value)
    return false;
  log_msg_set_value_with_type(msg, logmsg_handle::RESOURCE_SCHEMA_URL, value, len, LM_VT_STRING);

  /* .otel.scope.<...> */
  value = _get_protobuf_field(msg, logmsg_handle::RAW_SCOPE, &len);
  if (!value)
    return false;
  InstrumentationScope scope;
  if (!scope.ParsePartialFromArray(value, len))
    {
      msg_error("OpenTelemetry: Failed to deserialize .otel_raw.scope",
                evt_tag_msg_reference(msg));
      return false;
    }

  /* .otel.scope.name */
  _set_value(msg, logmsg_handle::SCOPE_NAME, scope.name(), LM_VT_STRING);

  /* .otel.scope.version */
  _set_value(msg, logmsg_handle::SCOPE_VERSION, scope.version(), LM_VT_STRING);

  /* .otel.scope.attributes */
  _add_repeated_KeyValue_fields(msg, ".otel.scope.attributes", scope.attributes());

  /* .otel.scope.dropped_attributes_count */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, scope.dropped_attributes_count());
  _set_value(msg, logmsg_handle::SCOPE_DROPPED_ATTRIBUTES_COUNT, number_buf, LM_VT_INTEGER);

  /* .otel.scope.schema_url */
  value = _get_string_field(msg, logmsg_handle::RAW_SCOPE_SCHEMA_URL, &len);
  if (!value)
    return false;
  log_msg_set_value_with_type(msg, logmsg_handle::SCOPE_SCHEMA_URL, value, len, LM_VT_STRING);

  return true;
}

static int
_map_severity_number_to_syslog_pri(SeverityNumber severity_number)
{
  switch (severity_number)
    {
    case SeverityNumber::SEVERITY_NUMBER_FATAL:
    case SeverityNumber::SEVERITY_NUMBER_FATAL4:
      return LOG_EMERG;
    case SeverityNumber::SEVERITY_NUMBER_FATAL2:
      return LOG_ALERT;
    case SeverityNumber::SEVERITY_NUMBER_FATAL3:
      return LOG_CRIT;
    case SeverityNumber::SEVERITY_NUMBER_ERROR:
    case SeverityNumber::SEVERITY_NUMBER_ERROR2:
    case SeverityNumber::SEVERITY_NUMBER_ERROR3:
    case SeverityNumber::SEVERITY_NUMBER_ERROR4:
      return LOG_ERR;
    case SeverityNumber::SEVERITY_NUMBER_WARN:
    case SeverityNumber::SEVERITY_NUMBER_WARN2:
    case SeverityNumber::SEVERITY_NUMBER_WARN3:
    case SeverityNumber::SEVERITY_NUMBER_WARN4:
      return LOG_WARNING;
    case SeverityNumber::SEVERITY_NUMBER_TRACE:
    case SeverityNumber::SEVERITY_NUMBER_TRACE2:
    case SeverityNumber::SEVERITY_NUMBER_TRACE3:
    case SeverityNumber::SEVERITY_NUMBER_TRACE4:
      return LOG_NOTICE;
    case SeverityNumber::SEVERITY_NUMBER_INFO:
    case SeverityNumber::SEVERITY_NUMBER_INFO2:
    case SeverityNumber::SEVERITY_NUMBER_INFO3:
    case SeverityNumber::SEVERITY_NUMBER_INFO4:
      return LOG_INFO;
    case SeverityNumber::SEVERITY_NUMBER_DEBUG:
    case SeverityNumber::SEVERITY_NUMBER_DEBUG2:
    case SeverityNumber::SEVERITY_NUMBER_DEBUG3:
    case SeverityNumber::SEVERITY_NUMBER_DEBUG4:
      return LOG_DEBUG;
    default:
      return LOG_INFO;
    }
}

static bool
_parse_log_record(LogMessage *msg)
{
  gssize len;
  const gchar *raw_value = _get_protobuf_field(msg, logmsg_handle::RAW_LOG, &len);
  if (!raw_value)
    return false;

  LogRecord log_record;
  if (!log_record.ParsePartialFromArray(raw_value, len))
    {
      msg_error("OpenTelemetry: Failed to deserialize .otel_raw.log",
                evt_tag_msg_reference(msg));
      return false;
    }

  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];

  /* .otel.type */
  log_msg_set_value_with_type(msg, logmsg_handle::TYPE, "log", -1, LM_VT_STRING);

  /* .otel.log.time_unix_nano */
  const guint64 time_unix_nano = log_record.time_unix_nano();
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, time_unix_nano);
  _set_value(msg, logmsg_handle::LOG_TIME_UNIX_NANO, number_buf, LM_VT_INTEGER);

  if (time_unix_nano != 0)
    {
      msg->timestamps[LM_TS_STAMP].ut_sec = time_unix_nano / 1000000000;
      msg->timestamps[LM_TS_STAMP].ut_usec = (time_unix_nano % 1000000000) / 1000;
    }

  /* .otel.log.observed_time_unix_nano */
  const guint64 observed_time_unix_nano = log_record.observed_time_unix_nano();
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, observed_time_unix_nano);
  _set_value(msg, logmsg_handle::LOG_OBSERVED_TIME_UNIX_NANO, number_buf, LM_VT_INTEGER);

  if (observed_time_unix_nano != 0)
    {
      msg->timestamps[LM_TS_RECVD].ut_sec = observed_time_unix_nano / 1000000000;
      msg->timestamps[LM_TS_RECVD].ut_usec = (observed_time_unix_nano % 1000000000) / 1000;
    }

  /* .otel.log.severity_number */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIi32, log_record.severity_number());
  _set_value(msg, logmsg_handle::LOG_SEVERITY_NUMBER, number_buf, LM_VT_INTEGER);

  msg->pri = LOG_MAKEPRI(LOG_USER, _map_severity_number_to_syslog_pri(log_record.severity_number()));

  /* .otel.log.severity_text */
  _set_value(msg, logmsg_handle::LOG_SEVERITY_TEXT, log_record.severity_text(), LM_VT_STRING);

  /* MESSAGE */
  LogMessageValueType body_lmvt;
  std::string body_str_buffer;
  const std::string &body_str = _serialize_AnyValue(log_record.body(), &body_lmvt, &body_str_buffer);
  _set_value(msg, LM_V_MESSAGE, body_str, body_lmvt);

  /* .otel.log.body */
  log_msg_set_value_indirect_with_type(msg, logmsg_handle::LOG_BODY, LM_V_MESSAGE, 0, body_str.length(), body_lmvt);

  /* .otel.log.attributes */
  _add_repeated_KeyValue_fields(msg, ".otel.log.attributes", log_record.attributes());

  /* .otel.log.dropped_attributes_count */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, log_record.dropped_attributes_count());
  _set_value(msg, logmsg_handle::LOG_DROPPED_ATTRIBUTES_COUNT, number_buf, LM_VT_INTEGER);

  /* .otel.log.flags */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, log_record.flags());
  _set_value(msg, logmsg_handle::LOG_FLAGS, number_buf, LM_VT_INTEGER);

  /* .otel.log.trace_id */
  _set_value(msg, logmsg_handle::LOG_TRACE_ID, log_record.trace_id(), LM_VT_BYTES);

  /* .otel.log.span_id */
  _set_value(msg, logmsg_handle::LOG_SPAN_ID, log_record.span_id(), LM_VT_BYTES);

  return true;
}

static void
_add_repeated_Exemplar_fields_with_prefix(LogMessage *msg, std::string &key_buffer, size_t key_prefix_length,
                                          const char *key, RepeatedPtrField<Exemplar> exemplars)
{
  key_buffer.resize(key_prefix_length);
  key_buffer.append(key);
  key_buffer.append(".");
  size_t length_with_dot = key_buffer.length();
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];

  uint64_t idx = 0;
  for (const Exemplar &exemplar : exemplars)
    {
      key_buffer.resize(length_with_dot);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t length_with_idx = key_buffer.length();

      /* <prefix>.<key>.<idx>.filtered_attributes.<...> */
      _add_repeated_KeyValue_fields_with_prefix(msg, key_buffer, length_with_idx, "filtered_attributes",
                                                exemplar.filtered_attributes());

      /* <prefix>.<key>.<idx>.time_unix_nano */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, exemplar.time_unix_nano());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "time_unix_nano", number_buf, LM_VT_INTEGER);

      /* <prefix>.<key>.<idx>.value */
      switch (exemplar.value_case())
        {
        case Exemplar::kAsDouble:
          g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), exemplar.as_double());
          _set_value_with_prefix(msg, key_buffer, length_with_idx, "value", number_buf, LM_VT_DOUBLE);
          break;
        case Exemplar::kAsInt:
          std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIi64, exemplar.as_int());
          _set_value_with_prefix(msg, key_buffer, length_with_idx, "value", number_buf, LM_VT_INTEGER);
          break;
        case Exemplar::VALUE_NOT_SET:
          break;
        default:
          msg_error("OpenTelemetry: unexpected Exemplar type", evt_tag_int("type", exemplar.value_case()));
        }

      /* <prefix>.<key>.<idx>.span_id */
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "span_id", exemplar.span_id(), LM_VT_BYTES);

      /* <prefix>.<key>.<idx>.trace_id */
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "trace_id", exemplar.trace_id(), LM_VT_BYTES);

      idx++;
    }
}

static void
_add_NumberDataPoints_fields_with_prefix(LogMessage *msg, std::string &key_buffer, size_t key_prefix_length,
                                         const char *key, RepeatedPtrField<NumberDataPoint> data_points)
{
  key_buffer.resize(key_prefix_length);
  key_buffer.append(key);
  key_buffer.append(".");
  size_t length_with_dot = key_buffer.length();
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];

  uint64_t idx = 0;
  for (const NumberDataPoint &data_point : data_points)
    {
      key_buffer.resize(length_with_dot);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t length_with_idx = key_buffer.length();

      /* <prefix>.<key>.<idx>.attributes */
      _add_repeated_KeyValue_fields_with_prefix(msg, key_buffer, length_with_idx, "attributes",
                                                data_point.attributes());

      /* <prefix>.<key>.<idx>.start_time_unix_nano */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, data_point.start_time_unix_nano());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "start_time_unix_nano", number_buf, LM_VT_INTEGER);

      /* <prefix>.<key>.<idx>.time_unix_nano */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, data_point.time_unix_nano());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "time_unix_nano", number_buf, LM_VT_INTEGER);

      /* <prefix>.<key>.<idx>.value */
      switch (data_point.value_case())
        {
        case NumberDataPoint::kAsDouble:
          g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), data_point.as_double());
          _set_value_with_prefix(msg, key_buffer, length_with_idx, "value", number_buf, LM_VT_DOUBLE);
          break;
        case NumberDataPoint::kAsInt:
          std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIi64, data_point.as_int());
          _set_value_with_prefix(msg, key_buffer, length_with_idx, "value", number_buf, LM_VT_INTEGER);
          break;
        case NumberDataPoint::VALUE_NOT_SET:
          break;
        default:
          msg_error("OpenTelemetry: unexpected NumberDataPoint type", evt_tag_int("type", data_point.value_case()));
        }

      /* <prefix>.<key>.<idx>.exemplars.<...> */
      _add_repeated_Exemplar_fields_with_prefix(msg, key_buffer, length_with_idx, "exemplars", data_point.exemplars());

      /* <prefix>.<key>.<idx>.flags */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, data_point.flags());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "flags", number_buf, LM_VT_INTEGER);

      idx++;
    }
}

static void
_add_NumberDataPoints_fields(LogMessage *msg, const char *key, RepeatedPtrField<NumberDataPoint> data_points)
{
  std::string key_buffer;
  _add_NumberDataPoints_fields_with_prefix(msg, key_buffer, 0, key, data_points);
}

static void
_add_metric_data_gauge_fields(LogMessage *msg, const Gauge &gauge)
{
  /* .otel.metric.data.gauge.data_points.<...> */
  _add_NumberDataPoints_fields(msg, ".otel.metric.data.gauge.data_points", gauge.data_points());
}

static void
_add_metric_data_sum_fields(LogMessage *msg, const Sum &sum)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];

  /* .otel.metric.data.sum.data_points.<...> */
  _add_NumberDataPoints_fields(msg, ".otel.metric.data.sum.data_points", sum.data_points());

  /* .otel.metric.data.sum.aggregation_temporality */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIi32, sum.aggregation_temporality());
  _set_value(msg, logmsg_handle::METRIC_DATA_SUM_AGGREGATION_TEMPORALITY, number_buf, LM_VT_INTEGER);

  /* .otel.metric.data.sum.is_monotonic */
  _set_value(msg, logmsg_handle::METRIC_DATA_SUM_IS_MONOTONIC, sum.is_monotonic() ? "true" : "false", LM_VT_BOOLEAN);
}

static void
_add_metric_data_histogram_fields(LogMessage *msg, const Histogram &histogram)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];

  /* .otel.metric.data.histogram.data_points.<...> */
  std::string key_buffer = ".otel.metric.data.histogram.data_points.";
  size_t length_with_dot = key_buffer.length();

  uint64_t idx = 0;
  for (const HistogramDataPoint &data_point : histogram.data_points())
    {
      key_buffer.resize(length_with_dot);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t length_with_idx = key_buffer.length();

      /* .otel.metric.data.histogram.data_points.<idx>.attributes.<...> */
      _add_repeated_KeyValue_fields_with_prefix(msg, key_buffer, length_with_idx, "attributes",
                                                data_point.attributes());

      /* .otel.metric.data.histogram.data_points.<idx>.start_time_unix_nano */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, data_point.start_time_unix_nano());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "start_time_unix_nano", number_buf, LM_VT_INTEGER);

      /* .otel.metric.data.histogram.data_points.<idx>.time_unix_nano */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, data_point.time_unix_nano());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "time_unix_nano", number_buf, LM_VT_INTEGER);

      /* .otel.metric.data.histogram.data_points.<idx>.count */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, data_point.count());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "count", number_buf, LM_VT_INTEGER);

      /* .otel.metric.data.histogram.data_points.<idx>.sum */
      g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), data_point.sum());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "sum", number_buf, LM_VT_DOUBLE);

      /* .otel.metric.data.histogram.data_points.<idx>.bucket_counts.<...> */
      key_buffer.resize(length_with_idx);
      key_buffer.append("bucket_counts.");
      size_t length_with_bucket_count = key_buffer.length();

      uint64_t bucket_count_idx = 0;
      for (uint64 bucket_count : data_point.bucket_counts())
        {
          key_buffer.resize(length_with_bucket_count);
          std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, bucket_count_idx);
          key_buffer.append(number_buf);

          /* .otel.metric.data.histogram.data_points.<idx>.bucket_counts.<idx> */
          std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, bucket_count);
          _set_value(msg, key_buffer.c_str(), number_buf, LM_VT_INTEGER);

          bucket_count_idx++;
        }

      /* .otel.metric.data.histogram.data_points.<idx>.explicit_bounds.<...> */
      key_buffer.resize(length_with_idx);
      key_buffer.append("explicit_bounds.");
      size_t length_with_explicit_bound = key_buffer.length();
      uint64_t explicit_bound_idx = 0;

      for (double explicit_bound : data_point.explicit_bounds())
        {
          key_buffer.resize(length_with_explicit_bound);
          std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, explicit_bound_idx);
          key_buffer.append(number_buf);

          /* .otel.metric.data.histogram.data_points.<idx>.explicit_bounds.<idx> */
          g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), explicit_bound);
          _set_value(msg, key_buffer.c_str(), number_buf, LM_VT_DOUBLE);

          explicit_bound_idx++;
        }

      /* .otel.metric.data.histogram.data_points.<idx>.exemplars.<...> */
      _add_repeated_Exemplar_fields_with_prefix(msg, key_buffer, length_with_idx, "exemplars", data_point.exemplars());

      /* .otel.metric.data.histogram.data_points.<idx>.flags */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, data_point.flags());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "flags", number_buf, LM_VT_INTEGER);

      /* .otel.metric.data.histogram.data_points.<idx>.min */
      g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), data_point.min());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "min", number_buf, LM_VT_DOUBLE);

      /* .otel.metric.data.histogram.data_points.<idx>.max */
      g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), data_point.max());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "max", number_buf, LM_VT_DOUBLE);

      idx++;
    }

  /* .otel.metric.data.histogram.aggregation_temporality */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIi32, histogram.aggregation_temporality());
  _set_value(msg, logmsg_handle::METRIC_DATA_HISTOGRAM_AGGREGATION_TEMPORALITY, number_buf, LM_VT_INTEGER);
}

static void
_add_Buckets_fields_with_prefix(LogMessage *msg, std::string &key_buffer, size_t key_prefix_length, const char *key,
                                const ExponentialHistogramDataPoint::Buckets &buckets)
{
  key_buffer.resize(key_prefix_length);
  key_buffer.append(key);
  key_buffer.append(".");
  size_t length_with_dot = key_buffer.length();
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];

  /* <prefix>.<key>.offset */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIi32, buckets.offset());
  _set_value_with_prefix(msg, key_buffer, length_with_dot, "offset", number_buf, LM_VT_INTEGER);

  /* <prefix>.<key>.bucket_counts.<...> */
  key_buffer.resize(length_with_dot);
  key_buffer.append("bucket_counts.");
  size_t length_with_bucket_counts = key_buffer.length();

  uint64_t idx = 0;
  for (uint64_t bucket_count : buckets.bucket_counts())
    {
      key_buffer.resize(length_with_bucket_counts);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, idx);
      key_buffer.append(number_buf);

      /* <prefix>.<key>.bucket_counts.<idx> */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, bucket_count);
      _set_value(msg, key_buffer.c_str(), number_buf, LM_VT_INTEGER);

      idx++;
    }
}

static void
_add_metric_data_exponential_histogram_fields(LogMessage *msg, const ExponentialHistogram &exponential_histogram)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];

  /* .otel.metric.data.exponential_histogram.data_points.<...> */
  std::string key_buffer = ".otel.metric.data.exponential_histogram.data_points.";
  size_t length_with_dot = key_buffer.length();

  uint64_t idx = 0;
  for (const ExponentialHistogramDataPoint &data_point : exponential_histogram.data_points())
    {
      key_buffer.resize(length_with_dot);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t length_with_idx = key_buffer.length();

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.attributes.<...> */
      _add_repeated_KeyValue_fields_with_prefix(msg, key_buffer, length_with_idx, "attributes",
                                                data_point.attributes());

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.start_time_unix_nano */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, data_point.start_time_unix_nano());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "start_time_unix_nano", number_buf, LM_VT_INTEGER);

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.time_unix_nano */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, data_point.time_unix_nano());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "time_unix_nano", number_buf, LM_VT_INTEGER);

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.count */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, data_point.count());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "count", number_buf, LM_VT_INTEGER);

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.sum */
      g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), data_point.sum());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "sum", number_buf, LM_VT_DOUBLE);

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.scale */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIi32, data_point.scale());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "scale", number_buf, LM_VT_INTEGER);

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.zero_count */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, data_point.zero_count());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "zero_count", number_buf, LM_VT_INTEGER);

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.positive.<...> */
      _add_Buckets_fields_with_prefix(msg, key_buffer, length_with_idx, "positive", data_point.positive());

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.negative.<...> */
      _add_Buckets_fields_with_prefix(msg, key_buffer, length_with_idx, "negative", data_point.negative());

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.flags */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, data_point.flags());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "flags", number_buf, LM_VT_INTEGER);

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.exemplars */
      _add_repeated_Exemplar_fields_with_prefix(msg, key_buffer, length_with_idx, "exemplars", data_point.exemplars());

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.min */
      g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), data_point.min());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "min", number_buf, LM_VT_DOUBLE);

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.max */
      g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), data_point.max());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "max", number_buf, LM_VT_DOUBLE);

      /* .otel.metric.data.exponential_histogram.data_points.<idx>.zero_threshold */
      g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), data_point.zero_threshold());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "zero_threshold", number_buf, LM_VT_DOUBLE);

      idx++;
    }

  /* .otel.metric.data.exponential_histogram.aggregation_temporality */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIi32, exponential_histogram.aggregation_temporality());
  _set_value(msg, logmsg_handle::METRIC_DATA_EXPONENTIAL_HISTOGRAM_AGGREGATION_TEMPORALITY, number_buf, LM_VT_INTEGER);
}

static void
_add_metric_data_summary_fields(LogMessage *msg, const Summary &summary)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];

  /* .otel.metric.data.summary.data_points.<...> */
  std::string key_buffer = ".otel.metric.data.summary.data_points.";
  size_t length_with_dot = key_buffer.length();

  uint64_t idx = 0;
  for (const SummaryDataPoint &data_point : summary.data_points())
    {
      key_buffer.resize(length_with_dot);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t length_with_idx = key_buffer.length();

      /* .otel.metric.data.summary.data_points.<idx>.attributes.<...> */
      _add_repeated_KeyValue_fields_with_prefix(msg, key_buffer, length_with_idx, "attributes",
                                                data_point.attributes());

      /* .otel.metric.data.summary.data_points.<idx>.start_time_unix_nano */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, data_point.start_time_unix_nano());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "start_time_unix_nano", number_buf, LM_VT_INTEGER);

      /* .otel.metric.data.summary.data_points.<idx>.time_unix_nano */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, data_point.time_unix_nano());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "time_unix_nano", number_buf, LM_VT_INTEGER);

      /* .otel.metric.data.summary.data_points.<idx>.count */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, data_point.count());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "count", number_buf, LM_VT_INTEGER);

      /* .otel.metric.data.summary.data_points.<idx>.sum */
      g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), data_point.sum());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "sum", number_buf, LM_VT_DOUBLE);

      /* .otel.metric.data.summary.data_points.<idx>.quantile_values.<...> */
      key_buffer.resize(length_with_idx);
      key_buffer.append("quantile_values.");
      size_t length_with_quantile_values = key_buffer.length();

      uint64_t quantile_value_idx = 0;
      for (const SummaryDataPoint::ValueAtQuantile &quantile_value : data_point.quantile_values())
        {
          key_buffer.resize(length_with_quantile_values);
          std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, quantile_value_idx);
          key_buffer.append(number_buf);
          key_buffer.append(".");
          size_t length_with_quantile_value_idx = key_buffer.length();

          /* .otel.metric.data.summary.data_points.<idx>.quantile_values.<idx>.quantile */
          g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), quantile_value.quantile());
          _set_value_with_prefix(msg, key_buffer, length_with_quantile_value_idx, "quantile", number_buf, LM_VT_DOUBLE);

          /* .otel.metric.data.summary.data_points.<idx>.quantile_values.<idx>.value */
          g_ascii_dtostr(number_buf, G_N_ELEMENTS(number_buf), quantile_value.value());
          _set_value_with_prefix(msg, key_buffer, length_with_quantile_value_idx, "value", number_buf, LM_VT_DOUBLE);

          quantile_value_idx++;
        }

      /* .otel.metric.data.summary.data_points.<idx>.flags */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, data_point.flags());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "flags", number_buf, LM_VT_INTEGER);

      idx++;
    }
}

static void
_add_metric_data_fields(LogMessage *msg, const Metric &metric)
{
  const char *type = nullptr;

  switch (metric.data_case())
    {
    case Metric::kGauge:
      type = "gauge";
      _add_metric_data_gauge_fields(msg, metric.gauge());
      break;
    case Metric::kSum:
      type = "sum";
      _add_metric_data_sum_fields(msg, metric.sum());
      break;
    case Metric::kHistogram:
      type = "histogram";
      _add_metric_data_histogram_fields(msg, metric.histogram());
      break;
    case Metric::kExponentialHistogram:
      type = "exponential_histogram";
      _add_metric_data_exponential_histogram_fields(msg, metric.exponential_histogram());
      break;
    case Metric::kSummary:
      type = "summary";
      _add_metric_data_summary_fields(msg, metric.summary());
      break;
    case Metric::DATA_NOT_SET:
      break;
    default:
      msg_error("OpenTelemetry: unexpected Metric type", evt_tag_int("type", metric.data_case()));
    }

  /* .otel.metric.data.type */
  if (type)
    log_msg_set_value_with_type(msg, logmsg_handle::METRIC_DATA_TYPE, type, -1, LM_VT_STRING);
}

static bool
_parse_metric(LogMessage *msg)
{
  gssize len;
  const gchar *raw_value = _get_protobuf_field(msg, logmsg_handle::RAW_METRIC, &len);
  if (!raw_value)
    return false;

  Metric metric;
  if (!metric.ParsePartialFromArray(raw_value, len))
    {
      msg_error("OpenTelemetry: Failed to deserialize .otel_raw.metric",
                evt_tag_msg_reference(msg));
      return false;
    }

  /* .otel.type */
  log_msg_set_value_with_type(msg, logmsg_handle::TYPE, "metric", -1, LM_VT_STRING);

  /* .otel.metric.name */
  _set_value(msg, logmsg_handle::METRIC_NAME, metric.name(), LM_VT_STRING);

  /* .otel.metric.description */
  _set_value(msg, logmsg_handle::METRIC_DESCRIPTION, metric.description(), LM_VT_STRING);

  /* .otel.metric.unit */
  _set_value(msg, logmsg_handle::METRIC_UNIT, metric.unit(), LM_VT_STRING);

  _add_metric_data_fields(msg, metric);

  return true;
}

static bool
_parse_span(LogMessage *msg)
{
  gssize len;
  const gchar *raw_value = _get_protobuf_field(msg, logmsg_handle::RAW_SPAN, &len);
  if (!raw_value)
    return false;

  Span span;
  if (!span.ParsePartialFromArray(raw_value, len))
    {
      msg_error("OpenTelemetry: Failed to deserialize .otel_raw.span",
                evt_tag_msg_reference(msg));
      return false;
    }

  /* .otel.type */
  log_msg_set_value_with_type(msg, logmsg_handle::TYPE, "span", -1, LM_VT_STRING);

  std::string key_buffer = ".otel.span.";
  size_t key_prefix_length = key_buffer.length();
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];

  /* .otel.span.trace_id */
  _set_value(msg, logmsg_handle::SPAN_TRACE_ID, span.trace_id(), LM_VT_BYTES);

  /* .otel.span.span_id */
  _set_value(msg, logmsg_handle::SPAN_SPAN_ID, span.span_id(), LM_VT_BYTES);

  /* .otel.span.trace_state */
  _set_value(msg, logmsg_handle::SPAN_TRACE_STATE, span.trace_state(), LM_VT_STRING);

  /* .otel.span.parent_span_id */
  _set_value(msg, logmsg_handle::SPAN_PARENT_SPAN_ID, span.parent_span_id(), LM_VT_BYTES);

  /* .otel.span.name */
  _set_value(msg, logmsg_handle::SPAN_NAME, span.name(), LM_VT_STRING);

  /* .otel.span.kind */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIi32, span.kind());
  _set_value(msg, logmsg_handle::SPAN_KIND, number_buf, LM_VT_INTEGER);

  /* .otel.span.start_time_unix_nano */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, span.start_time_unix_nano());
  _set_value(msg, logmsg_handle::SPAN_START_TIME_UNIX_NANO, number_buf, LM_VT_INTEGER);

  /* .otel.span.end_time_unix_nano */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, span.end_time_unix_nano());
  _set_value(msg, logmsg_handle::SPAN_END_TIME_UNIX_NANO, number_buf, LM_VT_INTEGER);

  /* .otel.span.attributes.<...> */
  _add_repeated_KeyValue_fields_with_prefix(msg, key_buffer, key_prefix_length, "attributes", span.attributes());

  /* .otel.span.dropped_attributes_count */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, span.dropped_attributes_count());
  _set_value(msg, logmsg_handle::SPAN_DROPPED_ATTRIBUTES_COUNT, number_buf, LM_VT_INTEGER);

  /* .otel.span.events.<...> */
  key_buffer.resize(key_prefix_length);
  key_buffer.append("events.");
  size_t length_with_events = key_buffer.length();

  uint64_t event_idx = 0;
  for (const Span::Event &event : span.events())
    {
      key_buffer.resize(length_with_events);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, event_idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t length_with_idx = key_buffer.length();

      /* .otel.span.events.<idx>.time_unix_nano */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, event.time_unix_nano());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "time_unix_nano", number_buf, LM_VT_INTEGER);

      /* .otel.span.events.<idx>.name */
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "name", event.name(), LM_VT_STRING);

      /* .otel.span.events.<idx>.attributes.<...> */
      _add_repeated_KeyValue_fields_with_prefix(msg, key_buffer, length_with_idx, "attributes", event.attributes());

      /* .otel.span.events.<idx>.dropped_attributes_count */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, event.dropped_attributes_count());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "dropped_attributes_count", number_buf,
                             LM_VT_INTEGER);

      event_idx++;
    }

  /* .otel.span.dropped_events_count */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, span.dropped_events_count());
  _set_value(msg, logmsg_handle::SPAN_DROPPED_EVENTS_COUNT, number_buf, LM_VT_INTEGER);

  /* .otel.span.links.<...> */
  key_buffer.resize(key_prefix_length);
  key_buffer.append("links.");
  size_t length_with_links = key_buffer.length();

  uint64_t link_idx = 0;
  for (const Span::Link &link : span.links())
    {
      key_buffer.resize(length_with_links);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, link_idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t length_with_idx = key_buffer.length();

      /* .otel.span.links.<idx>.trace_id */
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "trace_id", link.trace_id(), LM_VT_BYTES);

      /* .otel.span.links.<idx>.span_id */
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "span_id", link.span_id(), LM_VT_BYTES);

      /* .otel.span.links.<idx>.trace_state */
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "trace_state", link.trace_state(), LM_VT_STRING);

      /* .otel.span.links.<idx>.attributes.<...> */
      _add_repeated_KeyValue_fields_with_prefix(msg, key_buffer, length_with_idx, "attributes", link.attributes());

      /* .otel.span.links.<idx>.dropped_attributes_count */
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, link.dropped_attributes_count());
      _set_value_with_prefix(msg, key_buffer, length_with_idx, "dropped_attributes_count", number_buf,
                             LM_VT_INTEGER);

      link_idx++;
    }

  /* .otel.span.dropped_links_count */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, span.dropped_links_count());
  _set_value(msg, logmsg_handle::SPAN_DROPPED_LINKS_COUNT, number_buf, LM_VT_INTEGER);

  /* .otel.span.status.<...> */
  const Status &status = span.status();

  /* .otel.span.status.message */
  _set_value(msg, logmsg_handle::SPAN_STATUS_MESSAGE, status.message(), LM_VT_STRING);

  /* .otel.span.status.code */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIi32, status.code());
  _set_value(msg, logmsg_handle::SPAN_STATUS_CODE, number_buf, LM_VT_INTEGER);

  return true;
}

static void
_unset_raw_fields(LogMessage *msg)
{
  log_msg_unset_value(msg, logmsg_handle::RAW_RESOURCE);
  log_msg_unset_value(msg, logmsg_handle::RAW_RESOURCE_SCHEMA_URL);
  log_msg_unset_value(msg, logmsg_handle::RAW_SCOPE);
  log_msg_unset_value(msg, logmsg_handle::RAW_SCOPE_SCHEMA_URL);
  log_msg_unset_value(msg, logmsg_handle::RAW_TYPE);
  log_msg_unset_value(msg, logmsg_handle::RAW_LOG);
  log_msg_unset_value(msg, logmsg_handle::RAW_METRIC);
  log_msg_unset_value(msg, logmsg_handle::RAW_SPAN);
}

void
syslogng::grpc::otel::ProtobufParser::store_raw_metadata(LogMessage *msg, const ::grpc::string &peer,
                                                         const Resource &resource,
                                                         const std::string &resource_schema_url,
                                                         const InstrumentationScope &scope,
                                                         const std::string &scope_schema_url)
{
  std::string serialized;

  msg->saddr = _extract_saddr(peer);

  /* .otel_raw.resource */
  resource.SerializePartialToString(&serialized);
  _set_value(msg, logmsg_handle::RAW_RESOURCE, serialized, LM_VT_PROTOBUF);

  /* .otel_raw.resource_schema_url */
  _set_value(msg, logmsg_handle::RAW_RESOURCE_SCHEMA_URL, resource_schema_url, LM_VT_STRING);

  /* .otel_raw.scope */
  scope.SerializePartialToString(&serialized);
  _set_value(msg, logmsg_handle::RAW_SCOPE, serialized, LM_VT_PROTOBUF);

  /* .otel_raw.scope_schema_url */
  _set_value(msg, logmsg_handle::RAW_SCOPE_SCHEMA_URL, scope_schema_url, LM_VT_STRING);
}

void
syslogng::grpc::otel::ProtobufParser::store_raw(LogMessage *msg, const LogRecord &log_record)
{
  /* .otel_raw.type */
  _set_value(msg, logmsg_handle::RAW_TYPE, "log", LM_VT_STRING);

  /* .otel_raw.log */
  std::string serialized = log_record.SerializePartialAsString();
  _set_value(msg, logmsg_handle::RAW_LOG, serialized, LM_VT_PROTOBUF);
}

void
syslogng::grpc::otel::ProtobufParser::store_raw(LogMessage *msg, const Metric &metric)
{
  /* .otel_raw.type */
  _set_value(msg, logmsg_handle::RAW_TYPE, "metric", LM_VT_STRING);

  /* .otel_raw.metric */
  std::string serialized = metric.SerializePartialAsString();
  _set_value(msg, logmsg_handle::RAW_METRIC, serialized, LM_VT_PROTOBUF);
}

void
syslogng::grpc::otel::ProtobufParser::store_raw(LogMessage *msg, const Span &span)
{
  /* .otel_raw.type */
  _set_value(msg, logmsg_handle::RAW_TYPE, "span", LM_VT_STRING);

  /* .otel_raw.span */
  std::string serialized = span.SerializePartialAsString();
  _set_value(msg, logmsg_handle::RAW_SPAN, serialized, LM_VT_PROTOBUF);
}

static void
_nanosec_to_unix_time(uint64_t nanosec, UnixTime *unix_time)
{
  unix_time->ut_sec = nanosec / 1000000000;
  unix_time->ut_usec = (nanosec % 1000000000) / 1000;
}

static bool
_value_case_equals_or_error(LogMessage *msg, const KeyValue &kv, const AnyValue::ValueCase &expected_value_case)
{
  if (kv.value().value_case() != expected_value_case)
    {
      msg_error("OpenTelemetry: unexpected attribute value type, skipping",
                evt_tag_msg_reference(msg),
                evt_tag_str("name", kv.key().c_str()),
                evt_tag_int("type", kv.value().value_case()));
      return false;
    }

  return true;
}

void
syslogng::grpc::otel::ProtobufParser::set_syslog_ng_nv_pairs(LogMessage *msg, const KeyValueList &types)
{
  for (const KeyValue &nv_pairs_by_type : types.values())
    {
      LogMessageValueType log_msg_type;
      const std::string &type_as_str = nv_pairs_by_type.key();
      if (!log_msg_value_type_from_str(type_as_str.c_str(), &log_msg_type))
        {
          msg_debug("OpenTelemetry: unexpected attribute logmsg type, skipping",
                    evt_tag_msg_reference(msg),
                    evt_tag_str("type", type_as_str.c_str()));
          continue;
        }
      if (nv_pairs_by_type.value().value_case() != AnyValue::kKvlistValue)
        {
          msg_debug("OpenTelemetry: unexpected attribute, skipping",
                    evt_tag_msg_reference(msg),
                    evt_tag_str("key", type_as_str.c_str()));
          continue;
        }
      const KeyValueList &nv_pairs = nv_pairs_by_type.value().kvlist_value();

      for (const KeyValue &nv_pair : nv_pairs.values())
        {
          if (!_value_case_equals_or_error(msg, nv_pair, AnyValue::kBytesValue))
            continue;
          const std::string &name = nv_pair.key();
          const std::string &value = nv_pair.value().bytes_value();
          log_msg_set_value_by_name_with_type(msg, name.c_str(), value.c_str(), value.length(), log_msg_type);
        }
    }
}

void
syslogng::grpc::otel::ProtobufParser::set_syslog_ng_macros(LogMessage *msg, const KeyValueList &macros)
{
  for (const KeyValue &macro : macros.values())
    {
      const std::string &name = macro.key();

      if (name.compare("PRI") == 0)
        {
          if (macro.value().value_case() == AnyValue::kIntValue)
            msg->pri = macro.value().int_value();
          else if (macro.value().value_case() == AnyValue::kBytesValue)
            msg->pri = log_rewrite_set_pri_convert_pri(macro.value().bytes_value().c_str());
          else
            {
              msg_error("OpenTelemetry: unexpected attribute value type, skipping",
                        evt_tag_msg_reference(msg),
                        evt_tag_str("name", macro.key().c_str()),
                        evt_tag_int("type", macro.value().value_case()));
            }
        }
      else if (name.compare("TAGS") == 0)
        {
          if (!_value_case_equals_or_error(msg, macro, AnyValue::kBytesValue))
            continue;
          parse_syslog_ng_tags(msg, macro.value().bytes_value());
        }
      else if (name.compare("STAMP_GMTOFF") == 0)
        {
          if (!_value_case_equals_or_error(msg, macro, AnyValue::kIntValue))
            continue;
          msg->timestamps[LM_TS_STAMP].ut_gmtoff = (gint32) macro.value().int_value();
        }
      else if (name.compare("RECVD_GMTOFF") == 0)
        {
          if (!_value_case_equals_or_error(msg, macro, AnyValue::kIntValue))
            continue;
          msg->timestamps[LM_TS_RECVD].ut_gmtoff = (gint32) macro.value().int_value();
        }
      else
        {
          msg_debug("OpenTelemetry: unexpected attribute macro, skipping",
                    evt_tag_msg_reference(msg),
                    evt_tag_str("name", name.c_str()));
        }
    }
}

void
syslogng::grpc::otel::ProtobufParser::set_syslog_ng_address(LogMessage *msg, GSockAddr **sa,
                                                            const KeyValueList &addr_attributes)
{
  const std::string *addr_bytes = NULL;
  int port = 0;

  for (const KeyValue &attr : addr_attributes.values())
    {
      const std::string &name = attr.key();
      if (name.compare("addr") == 0)
        {
          if (!_value_case_equals_or_error(msg, attr, AnyValue::kBytesValue))
            continue;
          addr_bytes = &attr.value().bytes_value();
        }
      else if (name.compare("port") == 0)
        {
          if (!_value_case_equals_or_error(msg, attr, AnyValue::kIntValue))
            continue;
          port = attr.value().int_value();
        }
    }
  if (!addr_bytes)
    return;

  if (addr_bytes->length() == 4)
    {
      /* ipv4 */
      struct sockaddr_in sin;
      sin.sin_family = AF_INET;
      sin.sin_addr = *(struct in_addr *) addr_bytes->c_str();
      sin.sin_port = htons(port);
      *sa = g_sockaddr_inet_new2(&sin);
    }
#if SYSLOG_NG_ENABLE_IPV6
  else if (addr_bytes->length() == 16)
    {
      /* ipv6 */
      struct sockaddr_in6 sin6 = {0};
      sin6.sin6_family = AF_INET6;
      sin6.sin6_addr = *(struct in6_addr *) addr_bytes->c_str();
      sin6.sin6_port = htons(port);
      *sa = g_sockaddr_inet6_new2(&sin6);
    }
#endif
}

void
syslogng::grpc::otel::ProtobufParser::parse_syslog_ng_tags(LogMessage *msg, const std::string &tags_as_str)
{
  ListScanner list_scanner;
  list_scanner_init(&list_scanner);

  list_scanner_input_va(&list_scanner, tags_as_str.c_str(), NULL);
  while (list_scanner_scan_next(&list_scanner))
    {
      log_msg_set_tag_by_name(msg, list_scanner_get_current_value(&list_scanner));
    }

  list_scanner_deinit(&list_scanner);
}

void
syslogng::grpc::otel::ProtobufParser::store_syslog_ng(LogMessage *msg, const LogRecord &log_record)
{
  _nanosec_to_unix_time(log_record.time_unix_nano(), &msg->timestamps[LM_TS_STAMP]);
  _nanosec_to_unix_time(log_record.observed_time_unix_nano(), &msg->timestamps[LM_TS_RECVD]);

  for (const KeyValue &attr : log_record.attributes())
    {
      const std::string &key = attr.key();
      if (attr.value().value_case() != AnyValue::kKvlistValue)
        {
          msg_debug("OpenTelemetry: unexpected attribute, skipping",
                    evt_tag_msg_reference(msg),
                    evt_tag_str("key", key.c_str()));
          continue;
        }
      const KeyValueList &value = attr.value().kvlist_value();

      if (key.compare("n") == 0)
        {
          set_syslog_ng_nv_pairs(msg, value);
        }
      else if (key.compare("m") == 0)
        {
          set_syslog_ng_macros(msg, value);
        }
      else if (key.compare("sa") == 0)
        {
          set_syslog_ng_address(msg, &msg->saddr, value);
        }
      else if (key.compare("da") == 0)
        {
          set_syslog_ng_address(msg, &msg->daddr, value);
        }
      else
        {
          msg_debug("OpenTelemetry: unexpected attribute, skipping",
                    evt_tag_msg_reference(msg),
                    evt_tag_str("key", key.c_str()));
        }
    }
}

bool
syslogng::grpc::otel::ProtobufParser::is_syslog_ng_log_record(const Resource &resource,
    const std::string &resource_schema_url,
    const InstrumentationScope &scope,
    const std::string &scope_schema_url)
{
  return scope.name().compare("@syslog-ng") == 0;
}

bool
syslogng::grpc::otel::ProtobufParser::process(LogMessage *msg)
{
  msg_trace("OpenTelemetry: message processing started",
            evt_tag_msg_reference(msg));

  gssize len;
  LogMessageValueType log_msg_type;

  /* _parse_metadata() may invalidate the returned char pointer, so a copy is made with std::string */
  std::string type = log_msg_get_value_with_type(msg, logmsg_handle::RAW_TYPE, &len, &log_msg_type);

  if (log_msg_type == LM_VT_NULL)
    {
      /* Not an opentelemetry() message or it is a syslog-ng-otlp() message already parsed in the source */
      return true;
    }

  if (log_msg_type != LM_VT_STRING)
    {
      msg_error("OpenTelemetry: unexpected .otel_raw.type LogMessage type",
                evt_tag_msg_reference(msg),
                evt_tag_str("log_msg_type", log_msg_value_type_to_str(log_msg_type)));
      return false;
    }

  if (!_parse_metadata(msg, this->set_host))
    return false;

  if (type == "log")
    {
      if (!_parse_log_record(msg))
        return false;
    }
  else if (type == "metric")
    {
      if (!_parse_metric(msg))
        return false;
    }
  else if (type == "span")
    {
      if (!_parse_span(msg))
        return false;
    }
  else
    {
      msg_error("OpenTelemetry: unexpected .otel_raw.type",
                evt_tag_msg_reference(msg),
                evt_tag_str("type", type.c_str()));
      return false;
    }

  _unset_raw_fields(msg);

  return true;
}

static gboolean
_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input, gsize input_len)
{
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);
  return get_ProtobufParser(s)->process(msg);
}

static LogPipe *
_clone(LogPipe *s)
{
  OtelProtobufParser *self = (OtelProtobufParser *) s;
  OtelProtobufParser *cloned = (OtelProtobufParser *) otel_protobuf_parser_new(s->cfg);

  log_parser_clone_settings(&self->super, &cloned->super);

  return &cloned->super.super;
}

void
otel_protobuf_parser_set_hostname(LogParser *s, gboolean set_hostname)
{
  get_ProtobufParser(s)->set_hostname(set_hostname);
}

static void
_free(LogPipe *s)
{
  delete get_ProtobufParser(s);
  log_parser_free_method(s);
}

LogParser *
otel_protobuf_parser_new(GlobalConfig *cfg)
{
  OtelProtobufParser *self = g_new0(OtelProtobufParser, 1);

  self->cpp = new syslogng::grpc::otel::ProtobufParser();

  log_parser_init_instance(&self->super, cfg);
  self->super.super.free_fn = _free;
  self->super.super.clone = _clone;
  self->super.process = _process;

  return &self->super;
}
