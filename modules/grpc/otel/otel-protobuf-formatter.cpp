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
#include "scanner/list-scanner/list-scanner.h"
#include "compat/cpp-end.h"

#include <syslog.h>

using namespace google::protobuf;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;
using namespace opentelemetry::proto::logs::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::trace::v1;

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

static bool
_get_bool(LogMessage *msg, const gchar *name)
{
  gssize len;
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_by_name_with_type(msg, name, &len, &type);

  if (type != LM_VT_BOOLEAN)
    return false;

  gboolean b = false;
  if (!type_cast_to_boolean(value, len, &b, NULL))
    return false;

  return b;
}

static double
_get_double(LogMessage *msg, const gchar *name)
{
  gssize len;
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_by_name_with_type(msg, name, &len, &type);

  if (type != LM_VT_DOUBLE)
    return 0;

  gdouble d = 0;
  if (!type_cast_to_double(value, len, &d, NULL))
    return 0;

  return d;
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
      if (!type_cast_to_boolean(value, len, &b, &error))
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
      if (!type_cast_to_double(value, len, &d, &error))
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
      if (!type_cast_to_int64(value, len, &ll, &error))
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
    case LM_VT_LIST:
    {
      ArrayValue *array = any_value->mutable_array_value();

      ListScanner scanner;
      list_scanner_init(&scanner);
      list_scanner_input_string(&scanner, value, len);
      while (list_scanner_scan_next(&scanner))
        {
          array->add_values()->set_string_value(list_scanner_get_current_value(&scanner),
                                                list_scanner_get_current_value_len(&scanner));
        }
      list_scanner_deinit(&scanner);
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
  LogTemplateEvalOptions options = {&template_options, LTZ_LOCAL, -1, NULL, LM_VT_STRING};

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

void
ProtobufFormatter::get_metadata_for_syslog_ng(Resource &resource, std::string &resource_schema_url,
                                              InstrumentationScope &scope, std::string &scope_schema_url)
{
  scope.set_name("@syslog-ng");
  scope.set_version(VERSION_STR_CURRENT);
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

static uint64_t
_unix_time_to_nanosec(UnixTime *unix_time)
{
  return unix_time->ut_sec * 1000000000 + unix_time->ut_usec * 1000;
}

static bool
_is_number(const char *name)
{
  for (int i = 0; i < 3; i++)
    {
      if (!g_ascii_isdigit(name[i]))
        break;

      if (name[i+1] == '\0')
        return true;
    }

  return false;
}

static KeyValueList *
_create_types_kvlist_for_type(LogMessageValueType type, KeyValueList *types)
{
  KeyValue *new_type_kvlist_kv = types->add_values();
  new_type_kvlist_kv->set_key(log_msg_value_type_to_str(type));
  return new_type_kvlist_kv->mutable_value()->mutable_kvlist_value();;
}

static gboolean
_set_syslog_ng_nv_pairs_vp_helper(const gchar *name, LogMessageValueType type, const gchar *value, gsize value_len,
                                  gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  KeyValueList *types = (KeyValueList *) args[0];
  KeyValueList **lmvt_to_types_kvlist_mapping = (KeyValueList **) args[1];

  if (_is_number(name))
    return FALSE;

  KeyValueList *type_kvlist = lmvt_to_types_kvlist_mapping[type];
  if (type_kvlist == NULL)
    lmvt_to_types_kvlist_mapping[type] = type_kvlist = _create_types_kvlist_for_type(type, types);

  KeyValue *nv = type_kvlist->add_values();
  nv->set_key(name);
  nv->mutable_value()->set_bytes_value(value, value_len);

  return FALSE;
}

void
ProtobufFormatter::set_syslog_ng_nv_pairs(LogMessage *msg, LogRecord &log_record)
{
  /*
   * LogRecord -> attributes -> "n" -> "bool"   -> [ ("bool_name_1", "true"), ("bool_name_2", "false"), ... ]
   *                                -> "string" -> [ ("string_name_1", "string_value_1"), ... ]
   *                                -> ...
   */

  KeyValue *n = log_record.add_attributes();
  n->set_key("n");
  KeyValueList *types = n->mutable_value()->mutable_kvlist_value();
  KeyValueList *lmvt_to_types_kvlist_mapping[LM_VT_NONE] = { 0 };

  gpointer user_data[2];
  user_data[0] = types;
  user_data[1] = lmvt_to_types_kvlist_mapping;
  value_pairs_foreach(syslog_ng.vp, _set_syslog_ng_nv_pairs_vp_helper, msg, &syslog_ng.template_eval_options,
                      &user_data);
}

void
ProtobufFormatter::set_syslog_ng_macros(LogMessage *msg, LogRecord &log_record)
{
  /*
   * LogRecord -> attributes -> "m" -> [ ("PRI", "123"), ("TAGS", "foobar"), ... ]
   */

  KeyValue *m = log_record.add_attributes();
  m->set_key("m");
  KeyValueList *macros_kvlist = m->mutable_value()->mutable_kvlist_value();

  LogMessageValueType type;
  gssize len;
  const char *value;

  value = log_msg_get_value_by_name_with_type(msg, "PRI", &len, &type);
  KeyValue *pri_attr = macros_kvlist->add_values();
  pri_attr->set_key("PRI");
  pri_attr->mutable_value()->set_bytes_value(value, len);

  value = log_msg_get_value_by_name_with_type(msg, "TAGS", &len, &type);
  KeyValue *tags_attr = macros_kvlist->add_values();
  tags_attr->set_key("TAGS");
  tags_attr->mutable_value()->set_bytes_value(value, len);

  KeyValue *stamp_gmtoff = macros_kvlist->add_values();
  stamp_gmtoff->set_key("STAMP_GMTOFF");
  stamp_gmtoff->mutable_value()->set_int_value(msg->timestamps[LM_TS_STAMP].ut_gmtoff);

  KeyValue *recvd_gmtoff = macros_kvlist->add_values();
  recvd_gmtoff->set_key("RECVD_GMTOFF");
  recvd_gmtoff->mutable_value()->set_int_value(msg->timestamps[LM_TS_RECVD].ut_gmtoff);
}

void
ProtobufFormatter::format_syslog_ng(LogMessage *msg, LogRecord &log_record)
{
  log_record.set_time_unix_nano(_unix_time_to_nanosec(&msg->timestamps[LM_TS_STAMP]));
  log_record.set_observed_time_unix_nano(_unix_time_to_nanosec(&msg->timestamps[LM_TS_RECVD]));

  set_syslog_ng_nv_pairs(msg, log_record);
  set_syslog_ng_macros(msg, log_record);
}

void
ProtobufFormatter::add_exemplars(LogMessage *msg, std::string &key_buffer, RepeatedPtrField<Exemplar> *exemplars)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];
  size_t orig_len = key_buffer.length();

  gssize len;
  LogMessageValueType type;
  const gchar *value;
  GError *error = NULL;

  uint64_t idx = 0;
  while (true)
    {
      key_buffer.resize(orig_len);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t len_with_idx = key_buffer.length();

      key_buffer.append("value");
      value = log_msg_get_value_by_name_with_type(msg, key_buffer.c_str(), &len, &type);
      if (type == LM_VT_NULL)
        break;

      Exemplar *exemplar = exemplars->Add();

      if (type == LM_VT_INTEGER)
        {
          gint64 ll = 0;
          if (!type_cast_to_int64(value, len, &ll, &error))
            {
              msg_error("OpenTelemetry: Cannot parse integer value, falling back to 0",
                        evt_tag_str("name", key_buffer.c_str()),
                        evt_tag_str("value", value),
                        evt_tag_str("error", error->message));
              g_error_free(error);
            }
          exemplar->set_as_int(ll);
        }
      else if (type == LM_VT_DOUBLE)
        {
          gdouble d = 0;
          if (!type_cast_to_double(value, len, &d, &error))
            {
              msg_error("OpenTelemetry: Cannot parse double value, falling back to 0",
                        evt_tag_str("name", key_buffer.c_str()),
                        evt_tag_str("value", value),
                        evt_tag_str("error", error->message));
              g_error_free(error);
            }
          exemplar->set_as_double(d);
        }
      else
        {
          msg_error("OpenTelemetry: Cannot parse value, unexpected log message type, falling back to 0",
                    evt_tag_str("name", key_buffer.c_str()),
                    evt_tag_str("value", value),
                    evt_tag_str("type", log_msg_value_type_to_str(type)));
          exemplar->set_as_int(0);
        }

      key_buffer.resize(len_with_idx);
      key_buffer.append("filtered_attributes.");
      get_and_set_repeated_KeyValues(msg, key_buffer.c_str(), exemplar->mutable_filtered_attributes());

      key_buffer.resize(len_with_idx);
      key_buffer.append("time_unix_nano");
      exemplar->set_time_unix_nano(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("span_id");
      value = _get_bytes(msg, key_buffer.c_str(), &len);
      exemplar->set_span_id(value, len);

      key_buffer.resize(len_with_idx);
      key_buffer.append("trace_id");
      value = _get_bytes(msg, key_buffer.c_str(), &len);
      exemplar->set_trace_id(value, len);

      idx++;
    }
}

void
ProtobufFormatter::add_number_data_points(LogMessage *msg, const char *prefix,
                                          RepeatedPtrField<NumberDataPoint> *data_points)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];
  std::string key_buffer = prefix;
  size_t orig_len = key_buffer.length();

  gssize len;
  LogMessageValueType type;
  const gchar *value;
  GError *error = NULL;

  uint64_t idx = 0;
  while (true)
    {
      key_buffer.resize(orig_len);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t len_with_idx = key_buffer.length();

      key_buffer.append("value");
      value = log_msg_get_value_by_name_with_type(msg, key_buffer.c_str(), &len, &type);
      if (type == LM_VT_NULL)
        break;

      NumberDataPoint *data_point = data_points->Add();

      if (type == LM_VT_INTEGER)
        {
          gint64 ll = 0;
          if (!type_cast_to_int64(value, len, &ll, &error))
            {
              msg_error("OpenTelemetry: Cannot parse integer value, falling back to 0",
                        evt_tag_str("name", key_buffer.c_str()),
                        evt_tag_str("value", value),
                        evt_tag_str("error", error->message));
              g_error_free(error);
            }
          data_point->set_as_int(ll);
        }
      else if (type == LM_VT_DOUBLE)
        {
          gdouble d = 0;
          if (!type_cast_to_double(value, len, &d, &error))
            {
              msg_error("OpenTelemetry: Cannot parse double value, falling back to 0",
                        evt_tag_str("name", key_buffer.c_str()),
                        evt_tag_str("value", value),
                        evt_tag_str("error", error->message));
              g_error_free(error);
            }
          data_point->set_as_double(d);
        }
      else
        {
          msg_error("OpenTelemetry: Cannot parse value, unexpected log message type, falling back to 0",
                    evt_tag_str("name", key_buffer.c_str()),
                    evt_tag_str("value", value),
                    evt_tag_str("type", log_msg_value_type_to_str(type)));
          data_point->set_as_int(0);
        }

      key_buffer.resize(len_with_idx);
      key_buffer.append("attributes.");
      get_and_set_repeated_KeyValues(msg, key_buffer.c_str(), data_point->mutable_attributes());

      key_buffer.resize(len_with_idx);
      key_buffer.append("start_time_unix_nano");
      data_point->set_start_time_unix_nano(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("time_unix_nano");
      data_point->set_time_unix_nano(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("exemplars.");
      add_exemplars(msg, key_buffer, data_point->mutable_exemplars());

      key_buffer.resize(len_with_idx);
      key_buffer.append("flags");
      data_point->set_flags(_get_uint32(msg, key_buffer.c_str()));

      idx++;
    }
}

void
ProtobufFormatter::set_metric_gauge_values(LogMessage *msg, Gauge *gauge)
{
  add_number_data_points(msg, ".otel.metric.data.gauge.data_points.", gauge->mutable_data_points());
}

void
ProtobufFormatter::set_metric_sum_values(LogMessage *msg, Sum *sum)
{
  add_number_data_points(msg, ".otel.metric.data.sum.data_points.", sum->mutable_data_points());

  int32_t aggregation_temporality_int = _get_int32(msg, ".otel.metric.data.sum.aggregation_temporality");
  AggregationTemporality aggregation_temporality = AggregationTemporality_IsValid(aggregation_temporality_int) \
                                                   ? (AggregationTemporality) aggregation_temporality_int \
                                                   : AGGREGATION_TEMPORALITY_UNSPECIFIED;
  sum->set_aggregation_temporality(aggregation_temporality);

  sum->set_is_monotonic(_get_bool(msg, ".otel.metric.data.sum.is_monotonic"));
}

void
ProtobufFormatter::add_histogram_data_points(LogMessage *msg, const char *prefix,
                                             RepeatedPtrField<HistogramDataPoint> *data_points)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];
  std::string key_buffer = prefix;
  size_t orig_len = key_buffer.length();

  NVHandle handle;
  gssize len;
  LogMessageValueType type;
  const gchar *value;

  uint64_t idx = 0;
  while (true)
    {
      key_buffer.resize(orig_len);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t len_with_idx = key_buffer.length();

      key_buffer.append("count");
      handle = log_msg_get_value_handle(key_buffer.c_str());
      value = log_msg_get_value_if_set_with_type(msg, handle, &len, &type);
      if (!value)
        break;

      HistogramDataPoint *data_point = data_points->Add();

      data_point->set_count(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("attributes.");
      get_and_set_repeated_KeyValues(msg, key_buffer.c_str(), data_point->mutable_attributes());

      key_buffer.resize(len_with_idx);
      key_buffer.append("start_time_unix_nano");
      data_point->set_start_time_unix_nano(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("time_unix_nano");
      data_point->set_time_unix_nano(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("sum");
      data_point->set_sum(_get_double(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("bucket_counts.");
      size_t len_with_bucket_counts = key_buffer.length();
      uint64_t bucket_count_idx = 0;
      while (true)
        {
          key_buffer.resize(len_with_bucket_counts);
          std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, bucket_count_idx);
          key_buffer.append(number_buf);

          handle = log_msg_get_value_handle(key_buffer.c_str());
          value = log_msg_get_value_if_set_with_type(msg, handle, &len, &type);
          if (!value)
            break;

          data_point->add_bucket_counts(_get_uint64(msg, key_buffer.c_str()));

          bucket_count_idx++;
        }

      key_buffer.resize(len_with_idx);
      key_buffer.append("explicit_bounds.");
      size_t len_with_explicit_bounds = key_buffer.length();
      uint64_t explicit_bound_idx = 0;
      while (true)
        {
          key_buffer.resize(len_with_explicit_bounds);
          std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, explicit_bound_idx);
          key_buffer.append(number_buf);

          handle = log_msg_get_value_handle(key_buffer.c_str());
          value = log_msg_get_value_if_set_with_type(msg, handle, &len, &type);
          if (!value)
            break;

          data_point->add_explicit_bounds(_get_double(msg, key_buffer.c_str()));

          explicit_bound_idx++;
        }

      key_buffer.resize(len_with_idx);
      key_buffer.append("exemplars.");
      add_exemplars(msg, key_buffer, data_point->mutable_exemplars());

      key_buffer.resize(len_with_idx);
      key_buffer.append("flags");
      data_point->set_flags(_get_uint32(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("min");
      data_point->set_min(_get_double(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("max");
      data_point->set_max(_get_double(msg, key_buffer.c_str()));

      idx++;
    }
}

void
ProtobufFormatter::set_metric_histogram_values(LogMessage *msg, Histogram *histogram)
{
  add_histogram_data_points(msg, ".otel.metric.data.histogram.data_points.", histogram->mutable_data_points());

  int32_t aggregation_temporality_int = _get_int32(msg, ".otel.metric.data.histogram.aggregation_temporality");
  AggregationTemporality aggregation_temporality = AggregationTemporality_IsValid(aggregation_temporality_int) \
                                                   ? (AggregationTemporality) aggregation_temporality_int \
                                                   : AGGREGATION_TEMPORALITY_UNSPECIFIED;
  histogram->set_aggregation_temporality(aggregation_temporality);
}

static void
_add_buckets(LogMessage *msg, std::string &key_buffer, ExponentialHistogramDataPoint_Buckets *buckets)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];
  size_t orig_len = key_buffer.length();

  NVHandle handle;
  gssize len;
  LogMessageValueType type;
  const gchar *value;

  key_buffer.append("offset");
  buckets->set_offset(_get_int32(msg, key_buffer.c_str()));

  key_buffer.resize(orig_len);
  key_buffer.append("bucket_counts.");

  size_t len_with_bucket_counts = key_buffer.length();
  uint64_t idx = 0;
  while (true)
    {
      key_buffer.resize(len_with_bucket_counts);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, idx);
      key_buffer.append(number_buf);

      handle = log_msg_get_value_handle(key_buffer.c_str());
      value = log_msg_get_value_if_set_with_type(msg, handle, &len, &type);
      if (!value)
        break;

      buckets->add_bucket_counts(_get_uint64(msg, key_buffer.c_str()));

      idx++;
    }
}

void
ProtobufFormatter::add_exponential_histogram_data_points(LogMessage *msg, const char *prefix,
                                                         RepeatedPtrField<ExponentialHistogramDataPoint> *data_points)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];
  std::string key_buffer = prefix;
  size_t orig_len = key_buffer.length();

  NVHandle handle;
  gssize len;
  LogMessageValueType type;
  const gchar *value;

  uint64_t idx = 0;
  while (true)
    {
      key_buffer.resize(orig_len);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t len_with_idx = key_buffer.length();

      key_buffer.append("count");
      handle = log_msg_get_value_handle(key_buffer.c_str());
      value = log_msg_get_value_if_set_with_type(msg, handle, &len, &type);
      if (!value)
        break;

      ExponentialHistogramDataPoint *data_point = data_points->Add();

      data_point->set_count(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("attributes.");
      get_and_set_repeated_KeyValues(msg, key_buffer.c_str(), data_point->mutable_attributes());

      key_buffer.resize(len_with_idx);
      key_buffer.append("start_time_unix_nano");
      data_point->set_start_time_unix_nano(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("time_unix_nano");
      data_point->set_time_unix_nano(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("sum");
      data_point->set_sum(_get_double(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("scale");
      data_point->set_scale(_get_int32(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("zero_count");
      data_point->set_zero_count(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("positive.");
      _add_buckets(msg, key_buffer, data_point->mutable_positive());

      key_buffer.resize(len_with_idx);
      key_buffer.append("negative.");
      _add_buckets(msg, key_buffer, data_point->mutable_negative());

      key_buffer.resize(len_with_idx);
      key_buffer.append("flags");
      data_point->set_flags(_get_uint32(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("exemplars.");
      add_exemplars(msg, key_buffer, data_point->mutable_exemplars());

      key_buffer.resize(len_with_idx);
      key_buffer.append("min");
      data_point->set_min(_get_double(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("max");
      data_point->set_max(_get_double(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("zero_threshold");
      data_point->set_zero_threshold(_get_double(msg, key_buffer.c_str()));

      idx++;
    }
}

void
ProtobufFormatter::set_metric_exponential_histogram_values(LogMessage *msg, ExponentialHistogram *exponential_histogram)
{
  add_exponential_histogram_data_points(msg, ".otel.metric.data.exponential_histogram.data_points.",
                                        exponential_histogram->mutable_data_points());

  int32_t aggregation_temporality_int = _get_int32(msg,
                                                   ".otel.metric.data.exponential_histogram.aggregation_temporality");
  AggregationTemporality aggregation_temporality = AggregationTemporality_IsValid(aggregation_temporality_int) \
                                                   ? (AggregationTemporality) aggregation_temporality_int \
                                                   : AGGREGATION_TEMPORALITY_UNSPECIFIED;
  exponential_histogram->set_aggregation_temporality(aggregation_temporality);
}

void
ProtobufFormatter::add_summary_data_points(LogMessage *msg, const char *prefix,
                                           RepeatedPtrField<SummaryDataPoint> *data_points)
{
  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];
  std::string key_buffer = prefix;
  size_t orig_len = key_buffer.length();

  NVHandle handle;
  gssize len;
  LogMessageValueType type;
  const gchar *value;

  uint64_t idx = 0;
  while (true)
    {
      key_buffer.resize(orig_len);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t len_with_idx = key_buffer.length();

      key_buffer.append("count");
      handle = log_msg_get_value_handle(key_buffer.c_str());
      value = log_msg_get_value_if_set_with_type(msg, handle, &len, &type);
      if (!value)
        break;

      SummaryDataPoint *data_point = data_points->Add();

      data_point->set_count(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("start_time_unix_nano");
      data_point->set_start_time_unix_nano(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("time_unix_nano");
      data_point->set_time_unix_nano(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("sum");
      data_point->set_sum(_get_double(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("attributes.");
      get_and_set_repeated_KeyValues(msg, key_buffer.c_str(), data_point->mutable_attributes());

      key_buffer.resize(len_with_idx);
      key_buffer.append("quantile_values.");
      size_t len_with_quantile_values = key_buffer.length();
      uint64_t quantile_value_idx = 0;
      while (true)
        {
          key_buffer.resize(len_with_quantile_values);
          std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, quantile_value_idx);
          key_buffer.append(number_buf);
          key_buffer.append(".");
          size_t len_with_quantile_value_idx = key_buffer.length();

          key_buffer.append("quantile");
          handle = log_msg_get_value_handle(key_buffer.c_str());
          value = log_msg_get_value_if_set_with_type(msg, handle, &len, &type);
          if (!value)
            break;

          SummaryDataPoint_ValueAtQuantile *value_at_quantile = data_point->add_quantile_values();

          value_at_quantile->set_quantile(_get_double(msg, key_buffer.c_str()));

          key_buffer.resize(len_with_quantile_value_idx);
          key_buffer.append("value");
          value_at_quantile->set_value(_get_double(msg, key_buffer.c_str()));

          quantile_value_idx++;
        }

      key_buffer.resize(len_with_idx);
      key_buffer.append("flags");
      data_point->set_flags(_get_uint32(msg, key_buffer.c_str()));

      idx++;
    }
}

void
ProtobufFormatter::set_metric_summary_values(LogMessage *msg, Summary *summary)
{
  add_summary_data_points(msg, ".otel.metric.data.summary.data_points.", summary->mutable_data_points());
}

bool
ProtobufFormatter::format(LogMessage *msg, Metric &metric)
{
  gssize len;
  const gchar *value;

  value = _get_protobuf(msg, ".otel_raw.metric", &len);
  if (value)
    return metric.ParsePartialFromArray(value, len);

  LogMessageValueType type;

  value = _get_string(msg, ".otel.metric.name", &len);
  metric.set_name(value, len);

  value = _get_string(msg, ".otel.metric.description", &len);
  metric.set_description(value, len);

  value = _get_string(msg, ".otel.metric.unit", &len);
  metric.set_unit(value, len);

  value = log_msg_get_value_by_name_with_type(msg, ".otel.metric.data.type", &len, &type);
  if (type != LM_VT_STRING)
    {
      msg_error("OpenTelemetry: Failed to determine metric data type, invalid log message type",
                evt_tag_str("name", ".otel.metric.data.type"),
                evt_tag_str("value", value),
                evt_tag_str("log_msg_type", log_msg_value_type_to_str(type)));
      return false;
    }

  if (strncmp(value, "gauge", len) == 0)
    {
      set_metric_gauge_values(msg, metric.mutable_gauge());
    }
  else if (strncmp(value, "sum", len) == 0)
    {
      set_metric_sum_values(msg, metric.mutable_sum());
    }
  else if (strncmp(value, "histogram", len) == 0)
    {
      set_metric_histogram_values(msg, metric.mutable_histogram());
    }
  else if (strncmp(value, "exponential_histogram", len) == 0)
    {
      set_metric_exponential_histogram_values(msg, metric.mutable_exponential_histogram());
    }
  else if (strncmp(value, "summary", len) == 0)
    {
      set_metric_summary_values(msg, metric.mutable_summary());
    }
  else
    {
      msg_error("OpenTelemetry: Failed to determine metric data type, unexpected type",
                evt_tag_str("name", ".otel.metric.data.type"),
                evt_tag_str("value", value));
      return false;
    }

  return true;
}

bool
ProtobufFormatter::format(LogMessage *msg, Span &span)
{
  gssize len;
  const gchar *value;

  value = _get_protobuf(msg, ".otel_raw.span", &len);
  if (value)
    return span.ParsePartialFromArray(value, len);

  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];
  std::string key_buffer;
  NVHandle handle;
  LogMessageValueType type;

  value = _get_bytes(msg, ".otel.span.trace_id", &len);
  span.set_trace_id(value, len);

  value = _get_bytes(msg, ".otel.span.span_id", &len);
  span.set_span_id(value, len);

  value = _get_string(msg, ".otel.span.trace_state", &len);
  span.set_trace_state(value, len);

  value = _get_bytes(msg, ".otel.span.parent_span_id", &len);
  span.set_parent_span_id(value, len);

  value = _get_string(msg, ".otel.span.name", &len);
  span.set_name(value, len);

  int32_t kind_int = _get_int32(msg, ".otel.span.kind");
  Span_SpanKind kind = Span_SpanKind_IsValid(kind_int) ? (Span_SpanKind) kind_int : Span_SpanKind_SPAN_KIND_UNSPECIFIED;
  span.set_kind(kind);

  span.set_start_time_unix_nano(_get_uint64(msg, ".otel.span.start_time_unix_nano"));
  span.set_end_time_unix_nano(_get_uint64(msg, ".otel.span.end_time_unix_nano"));
  get_and_set_repeated_KeyValues(msg, ".otel.span.attributes.", span.mutable_attributes());
  span.set_dropped_attributes_count(_get_uint32(msg, ".otel.span.dropped_attributes_count"));

  key_buffer = ".otel.span.events.";
  size_t len_with_events = key_buffer.length();
  uint64_t event_idx = 0;
  while (true)
    {
      key_buffer.resize(len_with_events);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, event_idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t len_with_idx = key_buffer.length();

      key_buffer.append("time_unix_nano");
      handle = log_msg_get_value_handle(key_buffer.c_str());
      value = log_msg_get_value_if_set_with_type(msg, handle, &len, &type);
      if (!value)
        break;

      Span_Event *event = span.add_events();

      event->set_time_unix_nano(_get_uint64(msg, key_buffer.c_str()));

      key_buffer.resize(len_with_idx);
      key_buffer.append("name");
      value = _get_string(msg, key_buffer.c_str(), &len);
      event->set_name(value, len);

      key_buffer.resize(len_with_idx);
      key_buffer.append("attributes.");
      get_and_set_repeated_KeyValues(msg, key_buffer.c_str(), event->mutable_attributes());

      key_buffer.resize(len_with_idx);
      key_buffer.append("dropped_attributes_count");
      event->set_dropped_attributes_count(_get_uint32(msg, key_buffer.c_str()));

      event_idx++;
    }

  span.set_dropped_events_count(_get_uint32(msg, ".otel.span.dropped_events_count"));

  key_buffer = ".otel.span.links.";
  size_t len_with_links = key_buffer.length();
  uint64_t link_idx = 0;
  while (true)
    {
      key_buffer.resize(len_with_links);
      std::snprintf(number_buf, G_N_ELEMENTS(number_buf), "%" PRIu64, link_idx);
      key_buffer.append(number_buf);
      key_buffer.append(".");
      size_t len_with_idx = key_buffer.length();

      key_buffer.append("trace_id");
      handle = log_msg_get_value_handle(key_buffer.c_str());
      value = log_msg_get_value_if_set_with_type(msg, handle, &len, &type);
      if (!value)
        break;

      Span_Link *link = span.add_links();

      value = _get_bytes(msg, key_buffer.c_str(), &len);
      link->set_trace_id(value, len);

      key_buffer.resize(len_with_idx);
      key_buffer.append("span_id");
      value = _get_bytes(msg, key_buffer.c_str(), &len);
      link->set_span_id(value, len);

      key_buffer.resize(len_with_idx);
      key_buffer.append("trace_state");
      value = _get_string(msg, key_buffer.c_str(), &len);
      link->set_trace_state(value, len);

      key_buffer.resize(len_with_idx);
      key_buffer.append("attributes.");
      get_and_set_repeated_KeyValues(msg, key_buffer.c_str(), link->mutable_attributes());

      key_buffer.resize(len_with_idx);
      key_buffer.append("dropped_attributes_count");
      link->set_dropped_attributes_count(_get_uint32(msg, key_buffer.c_str()));

      link_idx++;
    }

  span.set_dropped_links_count(_get_uint32(msg, ".otel.span.dropped_links_count"));

  Status *status = span.mutable_status();
  value = _get_string(msg, ".otel.span.status.message", &len);
  status->set_message(value, len);

  int32_t code_int = _get_int32(msg, ".otel.span.status.code");
  Status_StatusCode code = Status_StatusCode_IsValid(code_int) ? (Status_StatusCode) code_int
                           : Status_StatusCode_STATUS_CODE_UNSET;
  status->set_code(code);

  return true;
}

syslogng::grpc::otel::ProtobufFormatter::ProtobufFormatter(GlobalConfig *cfg_)
  : cfg(cfg_)
{
  syslog_ng.vp = value_pairs_new(cfg);
  value_pairs_add_scope(syslog_ng.vp, "all-nv-pairs");
  value_pairs_set_include_bytes(syslog_ng.vp, TRUE);

  value_pairs_add_glob_pattern(syslog_ng.vp, "SOURCE", FALSE);

  log_template_options_defaults(&syslog_ng.template_options);
  syslog_ng.template_eval_options = {&syslog_ng.template_options, LTZ_LOCAL, -1, NULL, LM_VT_STRING};
}

syslogng::grpc::otel::ProtobufFormatter::~ProtobufFormatter()
{
  value_pairs_unref(syslog_ng.vp);
}
