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

#include "compat/cpp-start.h"
#include "logmsg/type-hinting.h"
#include "value-pairs/value-pairs.h"
#include "compat/cpp-end.h"

#include <syslog.h>

using namespace google::protobuf;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;
using namespace opentelemetry::proto::logs::v1;

using namespace syslogng::grpc::otel;

syslogng::grpc::otel::MessageType
syslogng::grpc::otel::get_message_type(LogMessage *msg)
{
  gssize len;
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_by_name_with_type(msg, ".otel_raw.type", &len, &type);

  if (type == LM_VT_NULL)
    value = log_msg_get_value_by_name_with_type(msg, ".otel.type", &len, &type);

  if (type != LM_VT_STRING)
    return MessageType::UNKNOWN;

  if (strncmp(value, "log", len) == 0)
    return MessageType::LOG;

  if (strncmp(value, "metric", len) == 0)
    return MessageType::METRIC;

  if (strncmp(value, "span", len) == 0)
    return MessageType::SPAN;

  return MessageType::UNKNOWN;
}

static uint64_t
_get_uint64(LogMessage *msg, const gchar *name)
{
  gssize len;
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_by_name_with_type(msg, name, &len, &type);

  if (type != LM_VT_INTEGER)
    return 0;

  return std::strtoull(value, nullptr, 10);
}

static int32_t
_get_int32(LogMessage *msg, const gchar *name)
{
  gssize len;
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_by_name_with_type(msg, name, &len, &type);

  if (type != LM_VT_INTEGER)
    return 0;

  return std::strtol(value, nullptr, 10);
}

static uint32_t
_get_uint32(LogMessage *msg, const gchar *name)
{
  gssize len;
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_by_name_with_type(msg, name, &len, &type);

  if (type != LM_VT_INTEGER)
    return 0;

  return std::strtoul(value, nullptr, 10);
}

static const gchar *
_get_string(LogMessage *msg, const gchar *name, gssize *len)
{
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_by_name_with_type(msg, name, len, &type);

  if (type != LM_VT_STRING)
    return "";

  return value;
}

static const gchar *
_get_bytes(LogMessage *msg, const gchar *name, gssize *len)
{
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_by_name_with_type(msg, name, len, &type);

  if (type != LM_VT_BYTES)
    {
      *len = 0;
      return NULL;
    }

  return value;
}

static const gchar *
_get_protobuf(LogMessage *msg, const gchar *name, gssize *len)
{
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_by_name_with_type(msg, name, len, &type);

  if (type != LM_VT_PROTOBUF)
    {
      *len = 0;
      return NULL;
    }

  return value;
}

static void
_set_AnyValue(const gchar *value, gssize len, LogMessageValueType type, AnyValue *any_value,
              const gchar *name_for_error_log)
{
  GError *error = NULL;

  switch (type)
    {
    case LM_VT_PROTOBUF:
      any_value->ParsePartialFromArray(value, len);
      break;
    case LM_VT_BYTES:
      any_value->set_bytes_value(value, len);
      break;
    case LM_VT_BOOLEAN:
    {
      gboolean b = FALSE;
      if (!type_cast_to_boolean(value, &b, &error))
        {
          msg_error("OpenTelemetry: Cannot parse boolean value, falling back to FALSE",
                    evt_tag_str("name", name_for_error_log),
                    evt_tag_str("value", value),
                    evt_tag_str("error", error->message));
          g_error_free(error);
        }
      any_value->set_bool_value(b);
      break;
    }
    case LM_VT_DOUBLE:
    {
      gdouble d = 0;
      if (!type_cast_to_double(value, &d, &error))
        {
          msg_error("OpenTelemetry: Cannot parse double value, falling back to 0",
                    evt_tag_str("name", name_for_error_log),
                    evt_tag_str("value", value),
                    evt_tag_str("error", error->message));
          g_error_free(error);
        }
      any_value->set_double_value(d);
      break;
    }
    case LM_VT_INTEGER:
    {
      gint64 ll = 0;
      if (!type_cast_to_int64(value, &ll, &error))
        {
          msg_error("OpenTelemetry: Cannot parse integer value, falling back to 0",
                    evt_tag_str("name", name_for_error_log),
                    evt_tag_str("value", value),
                    evt_tag_str("error", error->message));
          g_error_free(error);
        }
      any_value->set_int_value(ll);
      break;
    }
    case LM_VT_STRING:
      any_value->set_string_value(value, len);
      break;
    case LM_VT_NULL:
      break;
    default:
      msg_error("OpenTelemetry: Cannot parse value",
                evt_tag_str("name", name_for_error_log),
                evt_tag_str("value", value),
                evt_tag_str("type", log_msg_value_type_to_str(type)));
      break;
    }
}

static void
_get_and_set_AnyValue(LogMessage *msg, const gchar *name, AnyValue *any_value)
{
  LogMessageValueType type;
  gssize len;
  const gchar *value = log_msg_get_value_by_name_with_type(msg, name, &len, &type);
  _set_AnyValue(value, len, type, any_value, name);
}

gboolean
_set_KeyValue_vp_fn(const gchar *name, LogMessageValueType type, const gchar *value,
                    gsize value_len, gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  RepeatedPtrField<KeyValue> *key_values = (RepeatedPtrField<KeyValue> *) args[0];
  size_t prefix_len = *(size_t *) args[1];

  KeyValue *key_value = key_values->Add();
  key_value->set_key(name + prefix_len);
  _set_AnyValue(value, value_len, type, key_value->mutable_value(), name);

  return FALSE;
}

static SeverityNumber
_get_log_msg_severity_number(LogMessage *msg)
{
  static SeverityNumber mapping[] =
  {
    [LOG_EMERG] = SeverityNumber::SEVERITY_NUMBER_FATAL,
    [LOG_ALERT] = SeverityNumber::SEVERITY_NUMBER_FATAL2,
    [LOG_CRIT] = SeverityNumber::SEVERITY_NUMBER_FATAL3,
    [LOG_ERR] = SeverityNumber::SEVERITY_NUMBER_ERROR,
    [LOG_WARNING] = SeverityNumber::SEVERITY_NUMBER_WARN,
    [LOG_NOTICE] = SeverityNumber::SEVERITY_NUMBER_INFO2,
    [LOG_INFO] = SeverityNumber::SEVERITY_NUMBER_INFO,
    [LOG_DEBUG] = SeverityNumber::SEVERITY_NUMBER_DEBUG,
  };

  return mapping[LOG_PRI(msg->pri)];
}

void
ProtobufFormatter::get_and_set_repeated_KeyValues(LogMessage *msg, const char *prefix,
                                                  RepeatedPtrField<KeyValue> *key_values)
{
  ValuePairs *vp = value_pairs_new(cfg);
  value_pairs_set_include_bytes(vp, TRUE);

  std::string glob_pattern = prefix;
  size_t prefix_len = glob_pattern.length();
  glob_pattern.append("*");
  value_pairs_add_glob_pattern(vp, glob_pattern.c_str(), TRUE);

  LogTemplateOptions template_options;
  log_template_options_defaults(&template_options);
  LogTemplateEvalOptions options = {&template_options, LTZ_LOCAL, 11, NULL, LM_VT_STRING};

  gpointer user_data[2];
  user_data[0] = key_values;
  user_data[1] = &prefix_len;

  value_pairs_foreach(vp, _set_KeyValue_vp_fn, msg, &options, &user_data);

  value_pairs_unref(vp);
}

bool
ProtobufFormatter::get_resource_and_schema_url(LogMessage *msg, Resource &resource, std::string &schema_url)
{
  gssize len;
  const gchar *value;

  value = _get_protobuf(msg, ".otel_raw.resource", &len);
  if (value)
    {
      if (!resource.ParsePartialFromArray(value, len))
        return false;

      value = _get_string(msg, ".otel_raw.resource_schema_url", &len);
      schema_url.assign(value, len);

      return true;
    }

  resource.set_dropped_attributes_count(_get_uint32(msg, ".otel.resource.dropped_attributes_count"));

  get_and_set_repeated_KeyValues(msg, ".otel.resource.attributes.", resource.mutable_attributes());

  value = _get_string(msg, ".otel.resource.schema_url", &len);
  schema_url.assign(value, len);

  return true;
}

bool
ProtobufFormatter::get_scope_and_schema_url(LogMessage *msg, InstrumentationScope &scope, std::string &schema_url)
{
  gssize len;
  const gchar *value;

  value = _get_protobuf(msg, ".otel_raw.scope", &len);
  if (value)
    {
      if (!scope.ParsePartialFromArray(value, len))
        return false;

      value = _get_string(msg, ".otel_raw.scope_schema_url", &len);
      schema_url.assign(value, len);

      return true;
    }

  value = _get_string(msg, ".otel.scope.name", &len);
  scope.set_name(value, len);

  value = _get_string(msg, ".otel.scope.version", &len);
  scope.set_version(value, len);

  scope.set_dropped_attributes_count(_get_uint32(msg, ".otel.scope.dropped_attributes_count"));

  get_and_set_repeated_KeyValues(msg, ".otel.scope.attributes.", scope.mutable_attributes());

  value = _get_string(msg, ".otel.scope.schema_url", &len);
  schema_url.assign(value, len);

  return true;
}

bool
ProtobufFormatter::get_metadata(LogMessage *msg, Resource &resource, std::string &resource_schema_url,
                                InstrumentationScope &scope, std::string &scope_schema_url)
{
  return get_resource_and_schema_url(msg, resource, resource_schema_url) &&
         get_scope_and_schema_url(msg, scope, scope_schema_url);
}

bool
ProtobufFormatter::format(LogMessage *msg, LogRecord &log_record)
{
  gssize len;
  const gchar *value;

  value = _get_protobuf(msg, ".otel_raw.log", &len);
  if (value)
    return log_record.ParsePartialFromArray(value, len);

  log_record.set_time_unix_nano(_get_uint64(msg, ".otel.log.time_unix_nano"));
  log_record.set_observed_time_unix_nano(_get_uint64(msg, ".otel.log.observed_time_unix_nano"));

  int32_t severity_number_int = _get_int32(msg, ".otel.log.severity_number");
  SeverityNumber severity_number = SeverityNumber_IsValid(severity_number_int)
                                   ? (SeverityNumber) severity_number_int
                                   : SEVERITY_NUMBER_UNSPECIFIED;
  log_record.set_severity_number(severity_number);

  value = _get_string(msg, ".otel.log.severity_text", &len);
  log_record.set_severity_text(value, len);

  _get_and_set_AnyValue(msg, ".otel.log.body", log_record.mutable_body());

  get_and_set_repeated_KeyValues(msg, ".otel.log.attributes.", log_record.mutable_attributes());

  log_record.set_dropped_attributes_count(_get_uint32(msg, ".otel.log.dropped_attributes_count"));

  log_record.set_flags(_get_uint32(msg, ".otel.log.flags"));

  value = _get_bytes(msg, ".otel.log.trace_id", &len);
  log_record.set_trace_id(value, len);

  value = _get_bytes(msg, ".otel.log.span_id", &len);
  log_record.set_span_id(value, len);

  return true;
}

void
ProtobufFormatter::format_fallback(LogMessage *msg, LogRecord &log_record)
{
  log_record.set_time_unix_nano(msg->timestamps[LM_TS_STAMP].ut_sec * 1000000000 +
                                msg->timestamps[LM_TS_STAMP].ut_usec * 1000);
  log_record.set_observed_time_unix_nano(msg->timestamps[LM_TS_RECVD].ut_sec * 1000000000 +
                                         msg->timestamps[LM_TS_RECVD].ut_usec * 1000);
  log_record.set_severity_number(_get_log_msg_severity_number(msg));
  _get_and_set_AnyValue(msg, "MESSAGE", log_record.mutable_body());
}

syslogng::grpc::otel::ProtobufFormatter::ProtobufFormatter(GlobalConfig *cfg_)
  : cfg(cfg_)
{
}
