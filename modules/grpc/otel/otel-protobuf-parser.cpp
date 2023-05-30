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

#include <inttypes.h>

#include "otel-protobuf-parser.hpp"


using namespace google::protobuf;
using namespace opentelemetry::proto::resource::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::logs::v1;

static void
_set_value(LogMessage *msg, const char *key, const char *value, LogMessageValueType type)
{
  log_msg_set_value_by_name_with_type(msg, key, value, -1, type);
}

static void
_set_value(LogMessage *msg, const char *key, const std::string &value, LogMessageValueType type)
{
  log_msg_set_value_by_name_with_type(msg, key, value.c_str(), value.length(), type);
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
_serialize_AnyValue(const AnyValue &value, LogMessageValueType *type, std::string *buffer)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];

  switch (value.value_case())
    {
    case AnyValue::kArrayValue:
    case AnyValue::kKvlistValue:
      *type = LM_VT_PROTOBUF;
      value.SerializeToString(buffer);
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

static std::string
_extract_hostname(const grpc::string &peer)
{
  size_t first = peer.find_first_of(':');
  size_t last = peer.find_last_of(':');

  if (first != grpc::string::npos && last != grpc::string::npos)
    return peer.substr(first + 1, last - first - 1);

  return "";
}

void
syslogng::grpc::otel::protobuf_parser::set_metadata(LogMessage *msg, const ::grpc::string &peer,
                                                    const Resource &resource, const std::string &resource_schema_url,
                                                    const InstrumentationScope &scope,
                                                    const std::string &scope_schema_url)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];

  /* HOST */
  std::string hostname = _extract_hostname(peer);
  if (hostname.length())
    log_msg_set_value(msg, LM_V_HOST, hostname.c_str(), hostname.length());

  /* .otel.resource.attributes */
  _add_repeated_KeyValue_fields(msg, ".otel.resource.attributes", resource.attributes());

  /* .otel.resource.dropped_attributes_count */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, resource.dropped_attributes_count());
  _set_value(msg, ".otel.resource.dropped_attributes_count", number_buf, LM_VT_INTEGER);

  /* .otel.resource.schema_url */
  _set_value(msg, ".otel.resource.schema_url", resource_schema_url, LM_VT_STRING);

  /* .otel.scope.name */
  _set_value(msg, ".otel.scope.name", scope.name(), LM_VT_STRING);

  /* .otel.scope.version */
  _set_value(msg, ".otel.scope.version", scope.version(), LM_VT_STRING);

  /* .otel.scope.attributes */
  _add_repeated_KeyValue_fields(msg, ".otel.scope.attributes", scope.attributes());

  /* .otel.scope.dropped_attributes_count */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, scope.dropped_attributes_count());
  _set_value(msg, ".otel.scope.dropped_attributes_count", number_buf, LM_VT_INTEGER);

  /* .otel.scope.schema_url */
  _set_value(msg, ".otel.scope.schema_url", scope_schema_url, LM_VT_STRING);
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

void
syslogng::grpc::otel::protobuf_parser::parse(LogMessage *msg, const LogRecord &log_record)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];

  /* .otel.type */
  log_msg_set_value_by_name_with_type(msg, ".otel.type", "log", -1, LM_VT_STRING);

  /* .otel.log.time_unix_nano */
  const guint64 time_unix_nano = log_record.time_unix_nano();
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, time_unix_nano);
  _set_value(msg, ".otel.log.time_unix_nano", number_buf, LM_VT_INTEGER);

  if (time_unix_nano != 0)
    {
      msg->timestamps[LM_TS_STAMP].ut_sec = time_unix_nano / 1000000000;
      msg->timestamps[LM_TS_STAMP].ut_usec = (time_unix_nano % 1000000000) / 1000;
    }

  /* .otel.log.observed_time_unix_nano */
  const guint64 observed_time_unix_nano = log_record.observed_time_unix_nano();
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, observed_time_unix_nano);
  _set_value(msg, ".otel.log.observed_time_unix_nano", number_buf, LM_VT_INTEGER);

  if (observed_time_unix_nano != 0)
    {
      msg->timestamps[LM_TS_RECVD].ut_sec = observed_time_unix_nano / 1000000000;
      msg->timestamps[LM_TS_RECVD].ut_usec = (observed_time_unix_nano % 1000000000) / 1000;
    }

  /* .otel.log.severity_number */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIi32, log_record.severity_number());
  _set_value(msg, ".otel.log.severity_number", number_buf, LM_VT_INTEGER);

  msg->pri = LOG_MAKEPRI(LOG_USER, _map_severity_number_to_syslog_pri(log_record.severity_number()));

  /* .otel.log.severity_text */
  _set_value(msg, ".otel.log.severity_text", log_record.severity_text(), LM_VT_STRING);

  /* MESSAGE */
  LogMessageValueType body_lmvt;
  std::string body_str_buffer;
  const std::string &body_str = _serialize_AnyValue(log_record.body(), &body_lmvt, &body_str_buffer);
  _set_value(msg, LM_V_MESSAGE, body_str, body_lmvt);

  /* .otel.log.body */
  NVHandle body_handle = log_msg_get_value_handle(".otel.log.body");
  log_msg_set_value_indirect_with_type(msg, body_handle, LM_V_MESSAGE, 0, body_str.length(), body_lmvt);

  /* .otel.log.attributes */
  _add_repeated_KeyValue_fields(msg, ".otel.log.attributes", log_record.attributes());

  /* .otel.log.dropped_attributes_count */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, log_record.dropped_attributes_count());
  _set_value(msg, ".otel.log.dropped_attributes_count", number_buf, LM_VT_INTEGER);

  /* .otel.log.flags */
  std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu32, log_record.flags());
  _set_value(msg, ".otel.log.flags", number_buf, LM_VT_INTEGER);

  /* .otel.log.trace_id */
  _set_value(msg, ".otel.log.trace_id", log_record.trace_id(), LM_VT_BYTES);

  /* .otel.log.span_id */
  _set_value(msg, ".otel.log.span_id", log_record.span_id(), LM_VT_BYTES);
}
