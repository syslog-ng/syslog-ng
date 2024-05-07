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

#include "protobuf-field.hpp"

#include "compat/cpp-start.h"

#include "filterx/object-string.h"
#include "filterx/object-message-value.h"
#include "filterx/object-datetime.h"
#include "filterx/object-primitive.h"
#include "scratch-buffers.h"
#include "generic-number.h"
#include "filterx/object-json.h"
#include "filterx/object-null.h"
#include "compat/cpp-end.h"

#include <unistd.h>
#include <cstdio>
#include <memory>

using namespace syslogng::grpc::otel;

void
log_type_error(ProtoReflectors reflectors, const char *type)
{
  msg_error("protobuf-field: Failed to convert field, type is unsupported",
            evt_tag_str("field", reflectors.fieldDescriptor->name().c_str()),
            evt_tag_str("expected_type", reflectors.fieldDescriptor->type_name()),
            evt_tag_str("type", type));
}

float
double_to_float_safe(double val)
{
  if (val < (double)(-FLT_MAX))
    return -FLT_MAX;
  else if (val > (double)(FLT_MAX))
    return FLT_MAX;
  return (float)val;
}

/* C++ Implementations */

// ProtoReflectors reflectors
class BoolField : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(google::protobuf::Message *message, ProtoReflectors reflectors)
  {
    return filterx_boolean_new(reflectors.reflection->GetBool(*message, reflectors.fieldDescriptor));
  }
  bool FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object,
                           FilterXObject **assoc_object)
  {
    gboolean truthy = filterx_object_truthy(object);
    reflectors.reflection->SetBool(message, reflectors.fieldDescriptor, truthy);
    return true;
  }
};

class i32Field : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(google::protobuf::Message *message, ProtoReflectors reflectors)
  {
    return filterx_integer_new(gint32(reflectors.reflection->GetInt32(*message, reflectors.fieldDescriptor)));
  }
  bool FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object,
                           FilterXObject **assoc_object)
  {
    if (!filterx_object_is_type(object, &FILTERX_TYPE_NAME(integer)))
      {
        log_type_error(reflectors, object->type->name);
        return false;
      }
    GenericNumber gn = filterx_primitive_get_value(object);
    int32_t val = MAX(INT32_MIN, MIN(INT32_MAX, gn_as_int64(&gn)));
    reflectors.reflection->SetInt32(message, reflectors.fieldDescriptor, val);
    return true;
  }
};

class i64Field : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(google::protobuf::Message *message, ProtoReflectors reflectors)
  {
    return filterx_integer_new(gint64(reflectors.reflection->GetInt64(*message, reflectors.fieldDescriptor)));
  }
  bool FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object,
                           FilterXObject **assoc_object)
  {
    if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(integer)))
      {
        GenericNumber gn = filterx_primitive_get_value(object);
        int64_t val = gn_as_int64(&gn);
        reflectors.reflection->SetInt64(message, reflectors.fieldDescriptor, val);
        return true;
      }
    else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(datetime)))
      {
        const UnixTime utime = filterx_datetime_get_value(object);
        uint64_t unix_epoch = unix_time_to_unix_epoch(utime);
        reflectors.reflection->SetInt64(message, reflectors.fieldDescriptor, (int64_t)(unix_epoch));
        return true;
      }

    log_type_error(reflectors, object->type->name);
    return false;
  }
};

class u32Field : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(google::protobuf::Message *message, ProtoReflectors reflectors)
  {
    return filterx_integer_new(guint32(reflectors.reflection->GetUInt32(*message, reflectors.fieldDescriptor)));
  }
  bool FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object,
                           FilterXObject **assoc_object)
  {
    if (!filterx_object_is_type(object, &FILTERX_TYPE_NAME(integer)))
      {
        log_type_error(reflectors, object->type->name);
        return false;
      }
    GenericNumber gn = filterx_primitive_get_value(object);
    uint32_t val = MAX(0, MIN(UINT32_MAX, gn_as_int64(&gn)));
    reflectors.reflection->SetUInt32(message, reflectors.fieldDescriptor, val);
    return true;
  }
};

class u64Field : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(google::protobuf::Message *message, ProtoReflectors reflectors)
  {
    uint64_t val = reflectors.reflection->GetUInt64(*message, reflectors.fieldDescriptor);
    if (val > INT64_MAX)
      {
        msg_error("protobuf-field: exceeding FilterX number value range",
                  evt_tag_str("field", reflectors.fieldDescriptor->name().c_str()),
                  evt_tag_long("range_min", INT64_MIN),
                  evt_tag_long("range_max", INT64_MAX),
                  evt_tag_printf("current", "%" G_GUINT64_FORMAT, val));
        return NULL;
      }
    return filterx_integer_new(guint64(val));
  }
  bool FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object,
                           FilterXObject **assoc_object)
  {
    if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(integer)))
      {
        GenericNumber gn = filterx_primitive_get_value(object);
        uint64_t val = MAX(0, MIN(UINT64_MAX, (uint64_t) gn_as_int64(&gn)));
        reflectors.reflection->SetUInt64(message, reflectors.fieldDescriptor, val);
        return true;
      }
    else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(datetime)))
      {
        const UnixTime utime = filterx_datetime_get_value(object);
        uint64_t unix_epoch = unix_time_to_unix_epoch(utime);
        reflectors.reflection->SetUInt64(message, reflectors.fieldDescriptor, unix_epoch);
        return true;
      }

    log_type_error(reflectors, object->type->name);
    return false;
  }
};

class StringField : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(google::protobuf::Message *message, ProtoReflectors reflectors)
  {
    std::string bytesBuffer;
    const std::string &bytesRef = reflectors.reflection->GetStringReference(*message, reflectors.fieldDescriptor,
                                  &bytesBuffer);
    return filterx_string_new(bytesRef.c_str(), bytesRef.length());
  }
  bool FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object,
                           FilterXObject **assoc_object)
  {
    if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(string)))
      {
        gsize value_len = -1;
        const gchar *str = filterx_string_get_value(object, &value_len);
        std::string stdString(str, value_len);
        reflectors.reflection->SetString(message, reflectors.fieldDescriptor, stdString);
        return true;
      }
    else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(json_object)) ||
             filterx_object_is_type(object, &FILTERX_TYPE_NAME(json_array)))
      {
        const gchar *json_literal = filterx_json_to_json_literal(object);
        if (!json_literal)
          {
            msg_error("protobuf-field: json marshal error",
                      evt_tag_str("field", reflectors.fieldDescriptor->name().c_str()));
            return false;
          }
        reflectors.reflection->SetString(message, reflectors.fieldDescriptor, json_literal);
        return true;
      }
    log_type_error(reflectors, object->type->name);
    return false;
  }
};

class DoubleField : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(google::protobuf::Message *message, ProtoReflectors reflectors)
  {
    return filterx_double_new(gdouble(reflectors.reflection->GetDouble(*message, reflectors.fieldDescriptor)));
  }
  bool FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object,
                           FilterXObject **assoc_object)
  {
    GenericNumber gn = filterx_primitive_get_value(object);
    double val;

    if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(double)))
      {
        val = gn_as_double(&gn);
      }
    else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(integer)))
      {
        val = (double)(gn_as_int64(&gn));
      }
    else
      {
        log_type_error(reflectors, object->type->name);
        return false;
      }

    reflectors.reflection->SetDouble(message, reflectors.fieldDescriptor, val);
    return true;
  }
};

class FloatField : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(google::protobuf::Message *message, ProtoReflectors reflectors)
  {
    return filterx_double_new(gdouble(reflectors.reflection->GetFloat(*message, reflectors.fieldDescriptor)));
  }
  bool FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object,
                           FilterXObject **assoc_object)
  {
    GenericNumber gn = filterx_primitive_get_value(object);
    double val;

    if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(double)))
      {
        val = gn_as_double(&gn);
      }
    else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(integer)))
      {
        val = (double)(gn_as_int64(&gn));
      }
    else
      {
        log_type_error(reflectors, object->type->name);
        return false;
      }

    float floatVal = double_to_float_safe(val);
    reflectors.reflection->SetFloat(message, reflectors.fieldDescriptor, floatVal);
    return true;
  }
};

class BytesField : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(google::protobuf::Message *message, ProtoReflectors reflectors)
  {
    std::string bytesBuffer;
    const std::string &bytesRef = reflectors.reflection->GetStringReference(*message, reflectors.fieldDescriptor,
                                  &bytesBuffer);
    return filterx_bytes_new(bytesRef.c_str(), bytesRef.length());
  }
  bool FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object,
                           FilterXObject **assoc_object)
  {
    if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(bytes)))
      {
        gsize value_len = -1;
        const gchar *str = filterx_bytes_get_value(object, &value_len);
        std::string stdString(str, value_len);
        reflectors.reflection->SetString(message, reflectors.fieldDescriptor, stdString);

        return true;
      }
    else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(protobuf)))
      {
        gsize value_len = -1;
        const gchar *str = filterx_protobuf_get_value(object, &value_len);
        std::string stdString(str, value_len);
        reflectors.reflection->SetString(message, reflectors.fieldDescriptor, stdString);
        return true;
      }
    log_type_error(reflectors, object->type->name);
    return false;
  }
};

std::unique_ptr<ProtobufField> *syslogng::grpc::otel::all_protobuf_converters()
{
  static std::unique_ptr<ProtobufField> Converters[google::protobuf::FieldDescriptor::MAX_TYPE] =
  {
    std::make_unique<DoubleField>(),  //TYPE_DOUBLE = 1,    // double, exactly eight bytes on the wire.
    std::make_unique<FloatField>(),   //TYPE_FLOAT = 2,     // float, exactly four bytes on the wire.
    std::make_unique<i64Field>(),     //TYPE_INT64 = 3,     // int64, varint on the wire.  Negative numbers
    // take 10 bytes.  Use TYPE_SINT64 if negative
    // values are likely.
    std::make_unique<u64Field>(),     //TYPE_UINT64 = 4,    // uint64, varint on the wire.
    std::make_unique<i32Field>(),     //TYPE_INT32 = 5,     // int32, varint on the wire.  Negative numbers
    // take 10 bytes.  Use TYPE_SINT32 if negative
    // values are likely.
    std::make_unique<u64Field>(),     //TYPE_FIXED64 = 6,   // uint64, exactly eight bytes on the wire.
    std::make_unique<u32Field>(),     //TYPE_FIXED32 = 7,   // uint32, exactly four bytes on the wire.
    std::make_unique<BoolField>(),    //TYPE_BOOL = 8,      // bool, varint on the wire.
    std::make_unique<StringField>(),  //TYPE_STRING = 9,    // UTF-8 text.
    nullptr,                          //TYPE_GROUP = 10,    // Tag-delimited message.  Deprecated.
    nullptr,                          //TYPE_MESSAGE = 11,  // Length-delimited message.
    std::make_unique<BytesField>(),   //TYPE_BYTES = 12,     // Arbitrary byte array.
    std::make_unique<u32Field>(),     //TYPE_UINT32 = 13,    // uint32, varint on the wire
    nullptr,                          //TYPE_ENUM = 14,      // Enum, varint on the wire
    std::make_unique<i32Field>(),     //TYPE_SFIXED32 = 15,  // int32, exactly four bytes on the wire
    std::make_unique<i64Field>(),     //TYPE_SFIXED64 = 16,  // int64, exactly eight bytes on the wire
    std::make_unique<i32Field>(),     //TYPE_SINT32 = 17,    // int32, ZigZag-encoded varint on the wire
    std::make_unique<i64Field>(),     //TYPE_SINT64 = 18,    // int64, ZigZag-encoded varint on the wire
  };
  return Converters;
};

ProtobufField *syslogng::grpc::otel::protobuf_converter_by_type(google::protobuf::FieldDescriptor::Type fieldType)
{
  g_assert(fieldType <= google::protobuf::FieldDescriptor::MAX_TYPE && fieldType > 0);
  return all_protobuf_converters()[fieldType - 1].get();
}

std::string
syslogng::grpc::otel::extract_string_from_object(FilterXObject *object)
{
  gsize len;
  const gchar *key_c_str = filterx_string_get_value(object, &len);

  if (!key_c_str)
    key_c_str = filterx_message_value_get_value(object, &len);

  if (!key_c_str)
    throw std::runtime_error("not a string instance");

  return std::string{key_c_str, len};
}
