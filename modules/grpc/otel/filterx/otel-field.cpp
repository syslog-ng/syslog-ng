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

FilterXObject *
AnyField::FilterXObjectGetter(const Message &message, ProtoReflectors reflectors)
{
  if (reflectors.fieldDescriptor->type() == FieldDescriptor::TYPE_MESSAGE)
    {
      const Message &nestedMessage = reflectors.reflection->GetMessage(message, reflectors.fieldDescriptor);

      const AnyValue *anyValue;
      try
        {
          anyValue = dynamic_cast<const AnyValue *>(&nestedMessage);
        }
      catch(const std::bad_cast &e)
        {
          g_assert_not_reached();
        }

      return this->FilterXObjectDirectGetter(*anyValue);
    }

  msg_error("otel-field: Unexpected protobuf field type",
            evt_tag_str("name", reflectors.fieldDescriptor->name().c_str()),
            evt_tag_int("type", reflectors.fieldType));
  return nullptr;
}

bool
AnyField::FilterXObjectSetter(Message *message, ProtoReflectors reflectors, FilterXObject *object)
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

  return FilterXObjectDirectSetter(anyValue, object);
}

FilterXObject *
AnyField::FilterXObjectDirectGetter(const AnyValue &anyValue)
{
  ProtobufField *converter = nullptr;
  std::string typeFieldName;
  AnyValue::ValueCase valueCase = anyValue.value_case();

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
    default:
      break;
    }

  if (!converter)
    {
      msg_error("otel-field: AnyValue field type not yet implemented",
                evt_tag_int("value_case", valueCase));
      return nullptr;
    }

  return converter->Get(anyValue, typeFieldName.c_str());
}

bool
AnyField::FilterXObjectDirectSetter(AnyValue *anyValue, FilterXObject *object)
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
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(otel_kvlist)))
    {
      converter = &filterx::otel_kvlist_converter;
      typeFieldName = "kvlist_value";
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(otel_array)))
    {
      converter = &filterx::otel_array_converter;
      typeFieldName = "array_value";
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(json)))
    {
      converter = protobuf_converter_by_type(FieldDescriptor::TYPE_STRING);
      typeFieldName = "string_value";
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

  return converter->Set(anyValue, typeFieldName, object);
}

AnyField syslogng::grpc::otel::any_field_converter;

class OtelDatetimeConverter : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(const Message &message, ProtoReflectors reflectors)
  {
    uint64_t val = reflectors.reflection->GetUInt64(message, reflectors.fieldDescriptor);
    UnixTime utime = unix_time_from_unix_epoch(val);
    return filterx_datetime_new(&utime);
  }
  bool FilterXObjectSetter(Message *message, ProtoReflectors reflectors, FilterXObject *object)
  {
    if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(datetime)))
      {
        const UnixTime utime = filterx_datetime_get_value(object);
        uint64_t unix_epoch = unix_time_to_unix_epoch(utime);
        reflectors.reflection->SetUInt64(message, reflectors.fieldDescriptor, unix_epoch);
        return true;
      }
    else
      {
        msg_error("field type not yet implemented",
                  evt_tag_str("name", reflectors.fieldDescriptor->name().c_str()),
                  evt_tag_int("type", reflectors.fieldType));
        return false;
      }
  }
};

static OtelDatetimeConverter otel_datetime_converter;

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

  const FieldDescriptor::Type fieldType = fd->type();
  return otel_converter_by_type(fieldType);
}
