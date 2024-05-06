/*
 * Copyright (c) 2023 shifter
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

#include "syslog-ng.h"

#include "otel-field.hpp"
#include "object-otel-kvlist.hpp"
#include "object-otel-array.hpp"

#include "compat/cpp-start.h"
#include "filterx/filterx-object.h"
#include "filterx/object-string.h"
#include "filterx/object-datetime.h"
#include "filterx/object-primitive.h"
#include "scratch-buffers.h"
#include "generic-number.h"
#include "filterx/object-json.h"
#include "filterx/object-null.h"
#include "compat/cpp-end.h"

#include "opentelemetry/proto/logs/v1/logs.pb.h"


using namespace syslogng::grpc::otel;
using namespace google::protobuf;
using namespace opentelemetry::proto::logs::v1;

gpointer
grpc_otel_filterx_enum_construct(Plugin *self)
{
  static FilterXEnumDefinition enums[] =
  {
    { "SEVERITY_NUMBER_TRACE", SeverityNumber::SEVERITY_NUMBER_TRACE },
    { "SEVERITY_NUMBER_TRACE2", SeverityNumber::SEVERITY_NUMBER_TRACE2 },
    { "SEVERITY_NUMBER_TRACE3", SeverityNumber::SEVERITY_NUMBER_TRACE3 },
    { "SEVERITY_NUMBER_TRACE4", SeverityNumber::SEVERITY_NUMBER_TRACE4 },
    { "SEVERITY_NUMBER_DEBUG", SeverityNumber::SEVERITY_NUMBER_DEBUG },
    { "SEVERITY_NUMBER_DEBUG2", SeverityNumber::SEVERITY_NUMBER_DEBUG2 },
    { "SEVERITY_NUMBER_DEBUG3", SeverityNumber::SEVERITY_NUMBER_DEBUG3 },
    { "SEVERITY_NUMBER_DEBUG4", SeverityNumber::SEVERITY_NUMBER_DEBUG4 },
    { "SEVERITY_NUMBER_INFO", SeverityNumber::SEVERITY_NUMBER_INFO },
    { "SEVERITY_NUMBER_INFO2", SeverityNumber::SEVERITY_NUMBER_INFO2 },
    { "SEVERITY_NUMBER_INFO3", SeverityNumber::SEVERITY_NUMBER_INFO3 },
    { "SEVERITY_NUMBER_INFO4", SeverityNumber::SEVERITY_NUMBER_INFO4 },
    { "SEVERITY_NUMBER_WARN", SeverityNumber::SEVERITY_NUMBER_WARN },
    { "SEVERITY_NUMBER_WARN2", SeverityNumber::SEVERITY_NUMBER_WARN2 },
    { "SEVERITY_NUMBER_WARN3", SeverityNumber::SEVERITY_NUMBER_WARN3 },
    { "SEVERITY_NUMBER_WARN4", SeverityNumber::SEVERITY_NUMBER_WARN4 },
    { "SEVERITY_NUMBER_ERROR", SeverityNumber::SEVERITY_NUMBER_ERROR },
    { "SEVERITY_NUMBER_ERROR2", SeverityNumber::SEVERITY_NUMBER_ERROR2 },
    { "SEVERITY_NUMBER_ERROR3", SeverityNumber::SEVERITY_NUMBER_ERROR3 },
    { "SEVERITY_NUMBER_ERROR4", SeverityNumber::SEVERITY_NUMBER_ERROR4 },
    { "SEVERITY_NUMBER_FATAL", SeverityNumber::SEVERITY_NUMBER_FATAL },
    { "SEVERITY_NUMBER_FATAL2", SeverityNumber::SEVERITY_NUMBER_FATAL2 },
    { "SEVERITY_NUMBER_FATAL3", SeverityNumber::SEVERITY_NUMBER_FATAL3 },
    { "SEVERITY_NUMBER_FATAL4", SeverityNumber::SEVERITY_NUMBER_FATAL4 },
    { NULL },
  };

  return enums;
}

FilterXObject *
AnyField::FilterXObjectGetter(Message *message, ProtoReflectors reflectors)
{
  if (reflectors.fieldDescriptor->type() == FieldDescriptor::TYPE_MESSAGE)
    {
      Message *nestedMessage = reflectors.reflection->MutableMessage(message, reflectors.fieldDescriptor);

      AnyValue *anyValue;
      try
        {
          anyValue = dynamic_cast<AnyValue *>(nestedMessage);
        }
      catch(const std::bad_cast &e)
        {
          g_assert_not_reached();
        }

      return this->FilterXObjectDirectGetter(anyValue);
    }

  msg_error("otel-field: Unexpected protobuf field type",
            evt_tag_str("name", reflectors.fieldDescriptor->name().c_str()),
            evt_tag_int("type", reflectors.fieldType));
  return nullptr;
}

bool
AnyField::FilterXObjectSetter(Message *message, ProtoReflectors reflectors, FilterXObject *object,
                              FilterXObject **assoc_object)
{
  AnyValue *anyValue;
  try
    {
      anyValue = dynamic_cast<AnyValue *>(reflectors.reflection->MutableMessage(message, reflectors.fieldDescriptor));
    }
  catch(const std::bad_cast &e)
    {
      g_assert_not_reached();
    }

  return FilterXObjectDirectSetter(anyValue, object, assoc_object);
}

FilterXObject *
AnyField::FilterXObjectDirectGetter(AnyValue *anyValue)
{
  ProtobufField *converter = nullptr;
  std::string typeFieldName;
  AnyValue::ValueCase valueCase = anyValue->value_case();

  switch (valueCase)
    {
    case AnyValue::kStringValue:
      converter = protobuf_converter_by_type(FieldDescriptor::TYPE_STRING);
      typeFieldName = "string_value";
      break;
    case AnyValue::kBoolValue:
      converter = protobuf_converter_by_type(FieldDescriptor::TYPE_BOOL);
      typeFieldName = "bool_value";
      break;
    case AnyValue::kIntValue:
      converter = protobuf_converter_by_type(FieldDescriptor::TYPE_INT64);
      typeFieldName = "int_value";
      break;
    case AnyValue::kDoubleValue:
      converter = protobuf_converter_by_type(FieldDescriptor::TYPE_DOUBLE);
      typeFieldName = "double_value";
      break;
    case AnyValue::kBytesValue:
      converter = protobuf_converter_by_type(FieldDescriptor::TYPE_BYTES);
      typeFieldName = "bytes_value";
      break;
    case AnyValue::kKvlistValue:
      converter = &filterx::otel_kvlist_converter;
      typeFieldName = "kvlist_value";
      break;
    case AnyValue::kArrayValue:
      converter = &filterx::otel_array_converter;
      typeFieldName = "array_value";
      break;
    case AnyValue::VALUE_NOT_SET:
      return filterx_null_new();
    default:
      g_assert_not_reached();
    }

  return converter->Get(anyValue, typeFieldName.c_str());
}

bool
AnyField::FilterXObjectDirectSetter(AnyValue *anyValue, FilterXObject *object, FilterXObject **assoc_object)
{
  ProtobufField *converter = nullptr;
  const char *typeFieldName;

  if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(boolean)))
    {
      converter = protobuf_converter_by_type(FieldDescriptor::TYPE_BOOL);
      typeFieldName = "bool_value";
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(integer)))
    {
      converter = protobuf_converter_by_type(FieldDescriptor::TYPE_INT64);
      typeFieldName = "int_value";
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(double)))
    {
      converter = protobuf_converter_by_type(FieldDescriptor::TYPE_DOUBLE);
      typeFieldName = "double_value";
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(string)))
    {
      converter = protobuf_converter_by_type(FieldDescriptor::TYPE_STRING);
      typeFieldName = "string_value";
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(bytes)))
    {
      converter = protobuf_converter_by_type(FieldDescriptor::TYPE_BYTES);
      typeFieldName = "bytes_value";
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(protobuf)))
    {
      converter = protobuf_converter_by_type(FieldDescriptor::TYPE_BYTES);
      typeFieldName = "bytes_value";
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(dict)))
    {
      converter = &filterx::otel_kvlist_converter;
      typeFieldName = "kvlist_value";
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(list)))
    {
      converter = &filterx::otel_array_converter;
      typeFieldName = "array_value";
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(datetime)))
    {
      // notice int64_t (sfixed64) instead of uint64_t (fixed64) since anyvalue's int_value is int64_t
      converter = protobuf_converter_by_type(FieldDescriptor::TYPE_SFIXED64);
      typeFieldName = "int_value";
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(null)))
    {
      anyValue->clear_value();
      return true;
    }

  if (!converter)
    {
      msg_error("otel-field: FilterX type -> AnyValue field type conversion not yet implemented",
                evt_tag_str("type", object->type->name));
      return false;
    }

  return converter->Set(anyValue, typeFieldName, object, assoc_object);
}

AnyField syslogng::grpc::otel::any_field_converter;

class OtelDatetimeConverter : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(Message *message, ProtoReflectors reflectors)
  {
    uint64_t val = reflectors.reflection->GetUInt64(*message, reflectors.fieldDescriptor);
    UnixTime utime = unix_time_from_unix_epoch(val);
    return filterx_datetime_new(&utime);
  }
  bool FilterXObjectSetter(Message *message, ProtoReflectors reflectors, FilterXObject *object,
                           FilterXObject **assoc_object)
  {
    if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(datetime)))
      {
        const UnixTime utime = filterx_datetime_get_value(object);
        uint64_t unix_epoch = unix_time_to_unix_epoch(utime);
        reflectors.reflection->SetUInt64(message, reflectors.fieldDescriptor, unix_epoch);
        return true;
      }

    return protobuf_converter_by_type(reflectors.fieldDescriptor->type())->Set(message, reflectors.fieldDescriptor->name(),
           object, assoc_object);
  }
};

static OtelDatetimeConverter otel_datetime_converter;

class OtelSeverityNumberEnumConverter : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(Message *message, ProtoReflectors reflectors)
  {
    int value = reflectors.reflection->GetEnumValue(*message, reflectors.fieldDescriptor);
    return filterx_integer_new(value);
  }
  bool FilterXObjectSetter(Message *message, ProtoReflectors reflectors, FilterXObject *object,
                           FilterXObject **assoc_object)
  {
    if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(integer)))
      {
        int64_t value;
        filterx_integer_unwrap(object, &value);
        if (!SeverityNumber_IsValid((int) value))
          {
            msg_error("otel-field: Failed to set severity_number",
                      evt_tag_str("error", "Value is invalid"),
                      evt_tag_int("value", value));
            return false;
          }

        reflectors.reflection->SetEnumValue(message, reflectors.fieldDescriptor, (int) value);
        return true;
      }

    msg_error("otel-field: Failed to set severity_number",
              evt_tag_str("error", "Value type is invalid"),
              evt_tag_str("type", object->type->name));
    return false;
  }
};

static OtelSeverityNumberEnumConverter otel_severity_number_enum_converter;

ProtobufField *syslogng::grpc::otel::otel_converter_by_type(FieldDescriptor::Type fieldType)
{
  g_assert(fieldType <= FieldDescriptor::MAX_TYPE && fieldType > 0);
  if (fieldType == FieldDescriptor::TYPE_MESSAGE)
    {
      return &any_field_converter;
    }
  return all_protobuf_converters()[fieldType - 1].get();
}

ProtobufField *syslogng::grpc::otel::otel_converter_by_field_descriptor(const FieldDescriptor *fd)
{
  const std::string &fieldName = fd->name();
  if (fieldName.compare("time_unix_nano") == 0 ||
      fieldName.compare("observed_time_unix_nano") == 0)
    {
      return &otel_datetime_converter;
    }

  if (fieldName.compare("attributes") == 0)
    {
      return &filterx::otel_kvlist_converter;
    }

  if (fd->type() == FieldDescriptor::TYPE_ENUM)
    {
      return &otel_severity_number_enum_converter;
    }

  const FieldDescriptor::Type fieldType = fd->type();
  return otel_converter_by_type(fieldType);
}
