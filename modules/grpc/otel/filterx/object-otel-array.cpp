/*
 * Copyright (c) 2024 Attila Szakacs
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

#include "object-otel-array.hpp"
#include "otel-field.hpp"

#include "compat/cpp-start.h"
#include "filterx/object-primitive.h"
#include "filterx/object-string.h"
#include "compat/cpp-end.h"

#include <google/protobuf/reflection.h>
#include <stdexcept>

using namespace syslogng::grpc::otel::filterx;
using opentelemetry::proto::common::v1::AnyValue;
using opentelemetry::proto::common::v1::ArrayValue;

/* C++ Implementations */

Array::Array(FilterXOtelArray *s) : super(s)
{
}

Array::Array(FilterXOtelArray *s, FilterXObject *protobuf_object) : super(s)
{
  gsize length;
  const gchar *value = filterx_protobuf_get_value(protobuf_object, &length);

  if (!value)
    throw std::runtime_error("Argument is not a protobuf object");

  if (!array.ParsePartialFromArray(value, length))
    throw std::runtime_error("Failed to parse from protobuf object");
}

Array::Array(const Array &o, FilterXOtelArray *s) : super(s), array(o.array)
{
}

std::string
Array::marshal(void)
{
  return array.SerializePartialAsString();
}

bool
Array::set_subscript(FilterXObject *key, FilterXObject *value)
{
  if (!filterx_object_is_type(key, &FILTERX_TYPE_NAME(integer)))
    {
      msg_error("FilterX: Failed to get OTel Array element",
                evt_tag_str("error", "Key must be integer type"));
      return false;
    }

  GenericNumber gn = filterx_primitive_get_value(key);
  gint64 index = gn_as_int64(&gn);

  if (index >= array.values_size())
    {
      for (int i = 0; i < index + 1 - array.values_size(); i++)
        array.add_values();
    }

  AnyValue *any_value = array.mutable_values(index);

  return any_field_converter.FilterXObjectDirectSetter(any_value, value);
}

FilterXObject *
Array::get_subscript(FilterXObject *key)
{
  if (!filterx_object_is_type(key, &FILTERX_TYPE_NAME(integer)))
    {
      msg_error("FilterX: Failed to get OTel Array element",
                evt_tag_str("error", "Key must be integer type"));
      return NULL;
    }

  GenericNumber gn = filterx_primitive_get_value(key);
  gint64 index = gn_as_int64(&gn);

  if (index >= array.values_size())
    {
      msg_error("FilterX: Failed to get OTel Array element",
                evt_tag_int("index", index),
                evt_tag_str("error", "Index out of range"));
      return NULL;
    }

  const AnyValue &any_value = array.values(index);
  return any_field_converter.FilterXObjectDirectGetter(any_value);
}

const ArrayValue &
Array::get_value() const
{
  return array;
}

/* C Wrappers */

FilterXObject *
_filterx_otel_array_clone(FilterXObject *s)
{
  FilterXOtelArray *self = (FilterXOtelArray *) s;

  FilterXOtelArray *clone = g_new0(FilterXOtelArray, 1);
  filterx_object_init_instance(&clone->super, &FILTERX_TYPE_NAME(otel_array));

  try
    {
      clone->cpp = new Array(*self->cpp, self);
    }
  catch (const std::runtime_error &)
    {
      g_assert_not_reached();
    }

  return &clone->super;
}

static void
_free(FilterXObject *s)
{
  FilterXOtelArray *self = (FilterXOtelArray *) s;

  delete self->cpp;
  self->cpp = NULL;
}

static gboolean
_set_subscript(FilterXObject *s, FilterXObject *key, FilterXObject *new_value)
{
  FilterXOtelArray *self = (FilterXOtelArray *) s;

  return self->cpp->set_subscript(key, new_value);
}

static FilterXObject *
_get_subscript(FilterXObject *s, FilterXObject *key)
{
  FilterXOtelArray *self = (FilterXOtelArray *) s;

  return self->cpp->get_subscript(key);
}

static gboolean
_truthy(FilterXObject *s)
{
  return TRUE;
}

static gboolean
_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXOtelArray *self = (FilterXOtelArray *) s;

  std::string serialized = self->cpp->marshal();

  g_string_truncate(repr, 0);
  g_string_append_len(repr, serialized.c_str(), serialized.length());
  *t = LM_VT_PROTOBUF;
  return TRUE;
}

FilterXObject *
otel_array_new(GPtrArray *args)
{
  FilterXOtelArray *s = g_new0(FilterXOtelArray, 1);
  filterx_object_init_instance((FilterXObject *)s, &FILTERX_TYPE_NAME(otel_array));

  try
    {
      if (!args || args->len == 0)
        s->cpp = new Array(s);
      else if (args->len == 1)
        s->cpp = new Array(s, (FilterXObject *) g_ptr_array_index(args, 0));
      else
        throw std::runtime_error("Invalid number of arguments");
    }
  catch (const std::runtime_error &e)
    {
      msg_error("FilterX: Failed to create OTel Array object", evt_tag_str("error", e.what()));
      filterx_object_unref(&s->super);
      return NULL;
    }

  return &s->super;
}

gpointer
grpc_otel_filterx_array_construct_new(Plugin *self)
{
  return (gpointer) &otel_array_new;
}

FilterXObject *
OtelArrayField::FilterXObjectGetter(const google::protobuf::Message &message, ProtoReflectors reflectors)
{
  FilterXOtelArray *filterx_array = (FilterXOtelArray *) otel_array_new(NULL);

  try
    {
      const Message &nestedMessage = reflectors.reflection->GetMessage(message, reflectors.fieldDescriptor);
      const ArrayValue &array = dynamic_cast<const ArrayValue &>(nestedMessage);
      filterx_array->cpp->array.CopyFrom(array);
    }
  catch(const std::bad_cast &e)
    {
      g_assert_not_reached();
    }

  return &filterx_array->super;
}

bool
OtelArrayField::FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors,
                                    FilterXObject *object)
{
  if (!filterx_object_is_type(object, &FILTERX_TYPE_NAME(otel_array)))
    {
      msg_error("otel-array: Failed to convert field, type is unsupported",
                evt_tag_str("field", reflectors.fieldDescriptor->name().c_str()),
                evt_tag_str("expected_type", reflectors.fieldDescriptor->type_name()),
                evt_tag_str("type", object->type->name));
      return false;
    }

  FilterXOtelArray *filterx_array = (FilterXOtelArray *) object;
  ArrayValue *array;

  try
    {
      array = dynamic_cast<ArrayValue *>(reflectors.reflection->MutableMessage(message, reflectors.fieldDescriptor));
    }
  catch(const std::bad_cast &e)
    {
      g_assert_not_reached();
    }

  array->CopyFrom(filterx_array->cpp->get_value());

  return true;
}

OtelArrayField syslogng::grpc::otel::filterx::otel_array_converter;

FILTERX_DEFINE_TYPE(otel_array, FILTERX_TYPE_NAME(object),
                    .is_mutable = TRUE,
                    .marshal = _marshal,
                    .clone = _filterx_otel_array_clone,
                    .truthy = _truthy,
                    .get_subscript = _get_subscript,
                    .set_subscript = _set_subscript,
                    .free_fn = _free,
                   );
