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
#include "otel-logmsg-handles.hpp"

#include "compat/cpp-start.h"
#include "logmsg/type-hinting.h"
#include "value-pairs/value-pairs.h"
#include "scanner/list-scanner/list-scanner.h"
#include "compat/cpp-end.h"
#include "compat/inttypes.h"

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
  const gchar *value = log_msg_get_value_with_type(msg, logmsg_handle::RAW_TYPE, &len, &type);

  if (type == LM_VT_NULL)
    value = log_msg_get_value_with_type(msg, logmsg_handle::TYPE, &len, &type);

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
_get_uint64(LogMessage *msg, NVHandle handle)
{
  gssize len;
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_with_type(msg, handle, &len, &type);

  if (type != LM_VT_INTEGER)
    return 0;

  return std::strtoull(value, nullptr, 10);
}

static uint64_t
_get_uint64(LogMessage *msg, const gchar *name)
{
  return _get_uint64(msg, log_msg_get_value_handle(name));
}

static int32_t
_get_int32(LogMessage *msg, NVHandle handle)
{
  gssize len;
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_with_type(msg, handle, &len, &type);

  if (type != LM_VT_INTEGER)
    return 0;

  return std::strtol(value, nullptr, 10);
}

static int32_t
_get_int32(LogMessage *msg, const gchar *name)
{
  return _get_int32(msg, log_msg_get_value_handle(name));
}

static uint32_t
_get_uint32(LogMessage *msg, NVHandle handle)
{
  gssize len;
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_with_type(msg, handle, &len, &type);

  if (type != LM_VT_INTEGER)
    return 0;

  return std::strtoul(value, nullptr, 10);
}

static uint32_t
_get_uint32(LogMessage *msg, const gchar *name)
{
  return _get_uint32(msg, log_msg_get_value_handle(name));
}

static bool
_get_bool(LogMessage *msg, NVHandle handle)
{
  gssize len;
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_with_type(msg, handle, &len, &type);

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
_get_string(LogMessage *msg, NVHandle handle, gssize *len)
{
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_with_type(msg, handle, len, &type);

  if (type != LM_VT_STRING)
    return "";

  return value;
}

static const gchar *
_get_string(LogMessage *msg, const gchar *name, gssize *len)
{
  return _get_string(msg, log_msg_get_value_handle(name), len);
}

static const gchar *
_get_bytes(LogMessage *msg, NVHandle handle, gssize *len)
{
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_with_type(msg, handle, len, &type);

  if (type != LM_VT_BYTES)
    {
      *len = 0;
      return NULL;
    }

  return value;
}

static const gchar *
_get_bytes(LogMessage *msg, const gchar *name, gssize *len)
{
  return _get_bytes(msg, log_msg_get_value_handle(name), len);
}

static const gchar *
_get_protobuf(LogMessage *msg, NVHandle handle, gssize *len)
{
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_with_type(msg, handle, len, &type);

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
_get_and_set_AnyValue(LogMessage *msg, NVHandle handle, AnyValue *any_value)
{
  LogMessageValueType type;
  gssize len;
  const gchar *value = log_msg_get_value_with_type(msg, handle, &len, &type);
  _set_AnyValue(value, len, type, any_value, log_msg_get_value_name(handle, NULL));
}

gboolean
_set_KeyValue_log_msg_foreach_fn(NVHandle handle, const gchar *name, const gchar *value, gssize value_len,
                                 NVType type, gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  RepeatedPtrField<KeyValue> *key_values = (RepeatedPtrField<KeyValue> *) args[0];
  const char *prefix = (const char *) args[1];
  guint prefix_len = GPOINTER_TO_UINT(args[2]);

  if (strncmp(name, prefix, prefix_len) != 0)
    return FALSE;

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
  gpointer user_data[3];
  user_data[0] = key_values;
  user_data[1] = (gpointer) prefix;
  user_data[2] = GUINT_TO_POINTER(strlen(prefix));

  log_msg_values_foreach(msg, _set_KeyValue_log_msg_foreach_fn, user_data);
}

bool
ProtobufFormatter::get_resource_and_schema_url(LogMessage *msg, Resource &resource, std::string &schema_url)
{
  gssize len;
  const gchar *value;

  value = _get_protobuf(msg, logmsg_handle::RAW_RESOURCE, &len);
  if (value)
    {
      if (!resource.ParsePartialFromArray(value, len))
        return false;

      value = _get_string(msg, logmsg_handle::RAW_RESOURCE_SCHEMA_URL, &len);
      schema_url.assign(value, len);

      return true;
    }

  resource.set_dropped_attributes_count(_get_uint32(msg, logmsg_handle::RESOURCE_DROPPED_ATTRIBUTES_COUNT));

  get_and_set_repeated_KeyValues(msg, ".otel.resource.attributes.", resource.mutable_attributes());

  value = _get_string(msg, logmsg_handle::RESOURCE_SCHEMA_URL, &len);
  schema_url.assign(value, len);

  return true;
}

bool
ProtobufFormatter::get_scope_and_schema_url(LogMessage *msg, InstrumentationScope &scope, std::string &schema_url)
{
  gssize len;
  const gchar *value;

  value = _get_protobuf(msg, logmsg_handle::RAW_SCOPE, &len);
  if (value)
    {
      if (!scope.ParsePartialFromArray(value, len))
        return false;

      value = _get_string(msg, logmsg_handle::RAW_SCOPE_SCHEMA_URL, &len);
      schema_url.assign(value, len);

      return true;
    }

  value = _get_string(msg, logmsg_handle::SCOPE_NAME, &len);
  scope.set_name(value, len);

  value = _get_string(msg, logmsg_handle::SCOPE_VERSION, &len);
  scope.set_version(value, len);

  scope.set_dropped_attributes_count(_get_uint32(msg, logmsg_handle::SCOPE_DROPPED_ATTRIBUTES_COUNT));

  get_and_set_repeated_KeyValues(msg, ".otel.scope.attributes.", scope.mutable_attributes());

  value = _get_string(msg, logmsg_handle::SCOPE_SCHEMA_URL, &len);
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

  value = _get_protobuf(msg, logmsg_handle::RAW_LOG, &len);
  if (value)
    return log_record.ParsePartialFromArray(value, len);

  log_record.set_time_unix_nano(_get_uint64(msg, logmsg_handle::LOG_TIME_UNIX_NANO));
  log_record.set_observed_time_unix_nano(_get_uint64(msg, logmsg_handle::LOG_OBSERVED_TIME_UNIX_NANO));

  int32_t severity_number_int = _get_int32(msg, logmsg_handle::LOG_SEVERITY_NUMBER);
  SeverityNumber severity_number = SeverityNumber_IsValid(severity_number_int)
                                   ? (SeverityNumber) severity_number_int
                                   : SEVERITY_NUMBER_UNSPECIFIED;
  log_record.set_severity_number(severity_number);

  value = _get_string(msg, logmsg_handle::LOG_SEVERITY_TEXT, &len);
  log_record.set_severity_text(value, len);

  _get_and_set_AnyValue(msg, logmsg_handle::LOG_BODY, log_record.mutable_body());

  get_and_set_repeated_KeyValues(msg, ".otel.log.attributes.", log_record.mutable_attributes());

  log_record.set_dropped_attributes_count(_get_uint32(msg, logmsg_handle::LOG_DROPPED_ATTRIBUTES_COUNT));

  log_record.set_flags(_get_uint32(msg, logmsg_handle::LOG_FLAGS));

  value = _get_bytes(msg, logmsg_handle::LOG_TRACE_ID, &len);
  log_record.set_trace_id(value, len);

  value = _get_bytes(msg, logmsg_handle::LOG_SPAN_ID, &len);
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
  _get_and_set_AnyValue(msg, LM_V_MESSAGE, log_record.mutable_body());
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
_set_syslog_ng_nv_pairs_log_msg_foreach_fn(NVHandle handle, const gchar *name, const gchar *value, gssize value_len,
                                           NVType type, gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  KeyValueList *types = (KeyValueList *) args[0];
  KeyValueList **lmvt_to_types_kvlist_mapping = (KeyValueList **) args[1];

  if (_is_number(name))
    return FALSE;

  if (strcmp(name, "SOURCE") == 0)
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
  log_msg_values_foreach(msg, _set_syslog_ng_nv_pairs_log_msg_foreach_fn, user_data);
}

void
ProtobufFormatter::set_syslog_ng_address_attrs(GSockAddr *sa, KeyValueList *address_attrs, bool include_port)
{
  gchar addr_buf[32];
  gsize addr_len;

  if (!g_sockaddr_get_address(sa, (guint8 *) addr_buf, sizeof(addr_buf), &addr_len))
    return;

  KeyValue *addr = address_attrs->add_values();
  addr->set_key("addr");
  addr->mutable_value()->set_bytes_value(addr_buf, addr_len);

  if (include_port)
    {
      KeyValue *port = address_attrs->add_values();
      port->set_key("port");
      port->mutable_value()->set_int_value(g_sockaddr_get_port(sa));
    }
}

void
ProtobufFormatter::set_syslog_ng_addresses(LogMessage *msg, LogRecord &log_record)
{
  /* source address */

  if (msg->saddr)
    {
      KeyValue *sa = log_record.add_attributes();
      sa->set_key("sa");
      set_syslog_ng_address_attrs(msg->saddr, sa->mutable_value()->mutable_kvlist_value(), false);
    }

  if (msg->daddr)
    {
      /* dest address */
      KeyValue *da = log_record.add_attributes();
      da->set_key("da");
      set_syslog_ng_address_attrs(msg->daddr, da->mutable_value()->mutable_kvlist_value(), true);
    }
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

  KeyValue *pri_attr = macros_kvlist->add_values();
  pri_attr->set_key("PRI");
  pri_attr->mutable_value()->set_int_value(msg->pri);

  GString *tags_value = g_string_sized_new(64);
  log_msg_format_tags(msg, tags_value, FALSE);
  KeyValue *tags_attr = macros_kvlist->add_values();
  tags_attr->set_key("TAGS");
  tags_attr->mutable_value()->set_bytes_value(tags_value->str, tags_value->len);
  g_string_free(tags_value, TRUE);

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
  set_syslog_ng_addresses(msg, log_record);
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

  int32_t aggregation_temporality_int = _get_int32(msg, logmsg_handle::METRIC_DATA_SUM_AGGREGATION_TEMPORALITY);
  AggregationTemporality aggregation_temporality = AggregationTemporality_IsValid(aggregation_temporality_int) \
                                                   ? (AggregationTemporality) aggregation_temporality_int \
                                                   : AGGREGATION_TEMPORALITY_UNSPECIFIED;
  sum->set_aggregation_temporality(aggregation_temporality);

  sum->set_is_monotonic(_get_bool(msg, logmsg_handle::METRIC_DATA_SUM_IS_MONOTONIC));
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

  int32_t aggregation_temporality_int = _get_int32(msg, logmsg_handle::METRIC_DATA_HISTOGRAM_AGGREGATION_TEMPORALITY);
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
                                                   logmsg_handle::METRIC_DATA_EXPONENTIAL_HISTOGRAM_AGGREGATION_TEMPORALITY);
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

  value = _get_protobuf(msg, logmsg_handle::RAW_METRIC, &len);
  if (value)
    return metric.ParsePartialFromArray(value, len);

  LogMessageValueType type;

  value = _get_string(msg, logmsg_handle::METRIC_NAME, &len);
  metric.set_name(value, len);

  value = _get_string(msg, logmsg_handle::METRIC_DESCRIPTION, &len);
  metric.set_description(value, len);

  value = _get_string(msg, logmsg_handle::METRIC_UNIT, &len);
  metric.set_unit(value, len);

  value = log_msg_get_value_with_type(msg, logmsg_handle::METRIC_DATA_TYPE, &len, &type);
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

  value = _get_protobuf(msg, logmsg_handle::RAW_SPAN, &len);
  if (value)
    return span.ParsePartialFromArray(value, len);

  char number_buf[G_ASCII_DTOSTR_BUF_SIZE];
  std::string key_buffer;
  NVHandle handle;
  LogMessageValueType type;

  value = _get_bytes(msg, logmsg_handle::SPAN_TRACE_ID, &len);
  span.set_trace_id(value, len);

  value = _get_bytes(msg, logmsg_handle::SPAN_SPAN_ID, &len);
  span.set_span_id(value, len);

  value = _get_string(msg, logmsg_handle::SPAN_TRACE_STATE, &len);
  span.set_trace_state(value, len);

  value = _get_bytes(msg, logmsg_handle::SPAN_PARENT_SPAN_ID, &len);
  span.set_parent_span_id(value, len);

  value = _get_string(msg, logmsg_handle::SPAN_NAME, &len);
  span.set_name(value, len);

  int32_t kind_int = _get_int32(msg, logmsg_handle::SPAN_KIND);
  Span_SpanKind kind = Span_SpanKind_IsValid(kind_int) ? (Span_SpanKind) kind_int : Span_SpanKind_SPAN_KIND_UNSPECIFIED;
  span.set_kind(kind);

  span.set_start_time_unix_nano(_get_uint64(msg, logmsg_handle::SPAN_START_TIME_UNIX_NANO));
  span.set_end_time_unix_nano(_get_uint64(msg, logmsg_handle::SPAN_END_TIME_UNIX_NANO));
  get_and_set_repeated_KeyValues(msg, ".otel.span.attributes.", span.mutable_attributes());
  span.set_dropped_attributes_count(_get_uint32(msg, logmsg_handle::SPAN_DROPPED_ATTRIBUTES_COUNT));

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

  span.set_dropped_events_count(_get_uint32(msg, logmsg_handle::SPAN_DROPPED_EVENTS_COUNT));

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

  span.set_dropped_links_count(_get_uint32(msg, logmsg_handle::SPAN_DROPPED_LINKS_COUNT));

  Status *status = span.mutable_status();
  value = _get_string(msg, logmsg_handle::SPAN_STATUS_MESSAGE, &len);
  status->set_message(value, len);

  int32_t code_int = _get_int32(msg, logmsg_handle::SPAN_STATUS_CODE);
  Status_StatusCode code = Status_StatusCode_IsValid(code_int) ? (Status_StatusCode) code_int
                           : Status_StatusCode_STATUS_CODE_UNSET;
  status->set_code(code);

  return true;
}

syslogng::grpc::otel::ProtobufFormatter::ProtobufFormatter(GlobalConfig *cfg_)
  : cfg(cfg_)
{
}
