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

#include "object-otel-kvlist.hpp"
#include "otel-field.hpp"

#include "compat/cpp-start.h"
#include "filterx/object-string.h"
#include "filterx/object-null.h"
#include "compat/cpp-end.h"

#include <google/protobuf/reflection.h>
#include <stdexcept>

using namespace syslogng::grpc::otel::filterx;
using opentelemetry::proto::common::v1::KeyValueList;
using opentelemetry::proto::common::v1::AnyValue;

/* C++ Implementations */

KVList::KVList(FilterXOtelKVList *s) :
  super(s),
  repeated_kv(new RepeatedPtrField<KeyValue>()),
  borrowed(false)
{
}

KVList::KVList(FilterXOtelKVList *s, RepeatedPtrField<KeyValue> *k) :
  super(s),
  repeated_kv(k),
  borrowed(true)
{
}

KVList::KVList(FilterXOtelKVList *s, FilterXObject *protobuf_object) :
  super(s),
  repeated_kv(new RepeatedPtrField<KeyValue>()),
  borrowed(false)
{
  gsize length;
  const gchar *value = filterx_protobuf_get_value(protobuf_object, &length);

  if (!value)
    {
      delete repeated_kv;
      throw std::runtime_error("Argument is not a protobuf object");
    }

  KeyValueList temp_kvlist;
  if (!temp_kvlist.ParsePartialFromArray(value, length))
    {
      delete repeated_kv;
      throw std::runtime_error("Failed to parse from protobuf object");
    }

  repeated_kv->CopyFrom(*temp_kvlist.mutable_values());
}

KVList::KVList(const KVList &o, FilterXOtelKVList *s) :
  super(s),
  repeated_kv(new RepeatedPtrField<KeyValue>()),
  borrowed(false)
{
  repeated_kv->CopyFrom(*o.repeated_kv);
}

KVList::~KVList()
{
  if (!borrowed)
    delete repeated_kv;
}

std::string
KVList::marshal(void)
{
  KeyValueList temp_kvlist;
  temp_kvlist.mutable_values()->CopyFrom(*repeated_kv);
  return temp_kvlist.SerializePartialAsString();
}

KeyValue *
KVList::get_mutable_kv_for_key(const char *key) const
{
  for (int i = 0; i < repeated_kv->size(); i++)
    {
      KeyValue &possible_kv = repeated_kv->at(i);

      if (possible_kv.key().compare(key) == 0)
        return &possible_kv;
    }

  return nullptr;
}

bool
KVList::set_subscript(FilterXObject *key, FilterXObject **value)
{
  const gchar *key_c_str = filterx_string_get_value(key, NULL);
  if (!key_c_str)
    {
      msg_error("FilterX: Failed to set OTel KVList element",
                evt_tag_str("error", "Key must be string type"));
      return false;
    }

  ProtobufField *converter = otel_converter_by_type(FieldDescriptor::TYPE_MESSAGE);

  KeyValue *kv = get_mutable_kv_for_key(key_c_str);
  if (!kv)
    {
      kv = repeated_kv->Add();
      kv->set_key(key_c_str);
    }

  FilterXObject *assoc_object = NULL;
  if (!converter->Set(kv, "value", *value, &assoc_object))
    return false;

  filterx_object_unref(*value);
  *value = assoc_object;
  return true;
}

FilterXObject *
KVList::get_subscript(FilterXObject *key)
{
  const gchar *key_c_str = filterx_string_get_value(key, NULL);
  if (!key_c_str)
    {
      msg_error("FilterX: Failed to get OTel KVList element",
                evt_tag_str("error", "Key must be string type"));
      return NULL;
    }

  ProtobufField *converter = otel_converter_by_type(FieldDescriptor::TYPE_MESSAGE);
  KeyValue *kv = get_mutable_kv_for_key(key_c_str);
  if (!kv)
    {
      kv = repeated_kv->Add();
      kv->set_key(key_c_str);
    }

  return converter->Get(kv, "value");
}

bool
KVList::is_key_set(FilterXObject *key) const
{
  const gchar *key_c_str = filterx_string_get_value(key, NULL);
  if (!key_c_str)
    {
      msg_error("FilterX: Failed to check OTel KVList key",
                evt_tag_str("error", "Key must be string type"));
      return false;
    }

  return !!get_mutable_kv_for_key(key_c_str);
}

bool
KVList::unset_key(FilterXObject *key)
{
  const gchar *key_c_str = filterx_string_get_value(key, NULL);
  if (!key_c_str)
    {
      msg_error("FilterX: Failed to unset OTel KVList element",
                evt_tag_str("error", "Key must be string type"));
      return false;
    }

  for (int i = 0; i < repeated_kv->size(); i++)
    {
      KeyValue &possible_kv = repeated_kv->at(i);
      if (possible_kv.key().compare(key_c_str) == 0)
        {
          repeated_kv->DeleteSubrange(i, 1);
          return true;
        }
    }

  return true;
}

uint64_t
KVList::len() const
{
  return (uint64_t) repeated_kv->size();
}

bool
KVList::iter(FilterXDictIterFunc func, gpointer user_data) const
{
  ProtobufField *converter = otel_converter_by_type(FieldDescriptor::TYPE_MESSAGE);

  for (int i = 0; i < repeated_kv->size(); i++)
    {
      KeyValue &kv = repeated_kv->at(i);
      FilterXObject *key = filterx_string_new(kv.key().c_str(), kv.key().length());
      FilterXObject *value = converter->Get(&kv, "value");

      bool result = func(key, value, user_data);

      filterx_object_unref(key);
      filterx_object_unref(value);
      if (!result)
        return false;
    }

  return true;
}

const RepeatedPtrField<KeyValue> &
KVList::get_value() const
{
  return *repeated_kv;
}

/* C Wrappers */

static void
_free(FilterXObject *s)
{
  FilterXOtelKVList *self = (FilterXOtelKVList *) s;

  delete self->cpp;
  self->cpp = NULL;
}

static gboolean
_set_subscript(FilterXDict *s, FilterXObject *key, FilterXObject **new_value)
{
  FilterXOtelKVList *self = (FilterXOtelKVList *) s;

  return self->cpp->set_subscript(key, new_value);
}

static FilterXObject *
_get_subscript(FilterXDict *s, FilterXObject *key)
{
  FilterXOtelKVList *self = (FilterXOtelKVList *) s;

  return self->cpp->get_subscript(key);
}

static gboolean
_is_key_set(FilterXDict *s, FilterXObject *key)
{
  FilterXOtelKVList *self = (FilterXOtelKVList *) s;

  return self->cpp->is_key_set(key);
}

static gboolean
_unset_key(FilterXDict *s, FilterXObject *key)
{
  FilterXOtelKVList *self = (FilterXOtelKVList *) s;

  return self->cpp->unset_key(key);
}

static uint64_t
_len(FilterXDict *s)
{
  FilterXOtelKVList *self = (FilterXOtelKVList *) s;

  return self->cpp->len();
}

static gboolean
_iter(FilterXDict *s, FilterXDictIterFunc func, gpointer user_data)
{
  FilterXOtelKVList *self = (FilterXOtelKVList *) s;

  return self->cpp->iter(func, user_data);
}

static gboolean
_truthy(FilterXObject *s)
{
  return TRUE;
}

static gboolean
_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXOtelKVList *self = (FilterXOtelKVList *) s;

  std::string serialized = self->cpp->marshal();

  g_string_truncate(repr, 0);
  g_string_append_len(repr, serialized.c_str(), serialized.length());
  *t = LM_VT_PROTOBUF;
  return TRUE;
}

static void
_init_instance(FilterXOtelKVList *self)
{
  filterx_dict_init_instance(&self->super, &FILTERX_TYPE_NAME(otel_kvlist));

  self->super.get_subscript = _get_subscript;
  self->super.set_subscript = _set_subscript;
  self->super.is_key_set = _is_key_set;
  self->super.unset_key = _unset_key;
  self->super.len = _len;
  self->super.iter = _iter;
}

FilterXObject *
_filterx_otel_kvlist_clone(FilterXObject *s)
{
  FilterXOtelKVList *self = (FilterXOtelKVList *) s;

  FilterXOtelKVList *clone = g_new0(FilterXOtelKVList, 1);
  _init_instance(clone);

  try
    {
      clone->cpp = new KVList(*self->cpp, clone);
    }
  catch (const std::runtime_error &)
    {
      g_assert_not_reached();
    }

  return &clone->super.super;
}

FilterXObject *
filterx_otel_kvlist_new_from_args(GPtrArray *args)
{
  FilterXOtelKVList *self = g_new0(FilterXOtelKVList, 1);
  _init_instance(self);

  try
    {
      if (!args || args->len == 0)
        {
          self->cpp = new KVList(self);
        }
      else if (args->len == 1)
        {
          FilterXObject *arg = (FilterXObject *) g_ptr_array_index(args, 0);
          if (filterx_object_is_type(arg, &FILTERX_TYPE_NAME(dict)))
            {
              self->cpp = new KVList(self);
              if (!filterx_dict_merge(&self->super.super, arg))
                throw std::runtime_error("Failed to merge dict");
            }
          else
            {
              self->cpp = new KVList(self, arg);
            }
        }
      else
        {
          throw std::runtime_error("Invalid number of arguments");
        }
    }
  catch (const std::runtime_error &e)
    {
      msg_error("FilterX: Failed to create OTel KVList object", evt_tag_str("error", e.what()));
      filterx_object_unref(&self->super.super);
      return NULL;
    }

  return &self->super.super;
}

static FilterXObject *
_new_borrowed(RepeatedPtrField<KeyValue> *kvlist)
{
  FilterXOtelKVList *self = g_new0(FilterXOtelKVList, 1);
  _init_instance(self);

  self->cpp = new KVList(self, kvlist);

  return &self->super.super;
}

gpointer
grpc_otel_filterx_kvlist_construct_new(Plugin *self)
{
  return (gpointer) &filterx_otel_kvlist_new_from_args;
}

FilterXObject *
OtelKVListField::FilterXObjectGetter(google::protobuf::Message *message, ProtoReflectors reflectors)
{
  if (reflectors.fieldDescriptor->is_repeated())
    {
      auto repeated_fields = reflectors.reflection->MutableRepeatedPtrField<KeyValue>(message, reflectors.fieldDescriptor);
      return _new_borrowed(repeated_fields);
    }

  try
    {
      Message *nestedMessage = reflectors.reflection->MutableMessage(message, reflectors.fieldDescriptor);
      KeyValueList *kvlist = dynamic_cast<KeyValueList *>(nestedMessage);
      return _new_borrowed(kvlist->mutable_values());
    }
  catch(const std::bad_cast &e)
    {
      g_assert_not_reached();
    }
}

static RepeatedPtrField<KeyValue> *
_get_repeated_kv(google::protobuf::Message *message, syslogng::grpc::otel::ProtoReflectors reflectors)
{
  RepeatedPtrField<KeyValue> *repeated_kv;

  if (reflectors.fieldDescriptor->is_repeated())
    {
      try
        {
          repeated_kv = reflectors.reflection->MutableRepeatedPtrField<KeyValue>(message, reflectors.fieldDescriptor);
        }
      catch(const std::bad_cast &e)
        {
          g_assert_not_reached();
        }
    }
  else
    {
      KeyValueList *kvlist;
      try
        {
          kvlist = dynamic_cast<KeyValueList *>(reflectors.reflection->MutableMessage(message, reflectors.fieldDescriptor));
          repeated_kv = kvlist->mutable_values();
        }
      catch(const std::bad_cast &e)
        {
          g_assert_not_reached();
        }
    }

  return repeated_kv;
}

static gboolean
_add_elem_to_repeated_kv(FilterXObject *key_obj, FilterXObject *value_obj, gpointer user_data)
{
  RepeatedPtrField<KeyValue> *repeated_kv = (RepeatedPtrField<KeyValue> *) user_data;

  /* FilterX strings are always NUL terminated. */
  const gchar *key = filterx_string_get_value(key_obj, NULL);
  if (!key)
    return FALSE;

  KeyValue *kv = repeated_kv->Add();
  kv->set_key(key);

  FilterXObject *assoc_object = NULL;
  if (!syslogng::grpc::otel::any_field_converter.FilterXObjectDirectSetter(kv->mutable_value(), value_obj, &assoc_object))
    return false;

  filterx_object_unref(assoc_object);
  return true;
}

static bool
_set_kvlist_field_from_dict(google::protobuf::Message *message, syslogng::grpc::otel::ProtoReflectors reflectors,
                            FilterXObject *object, FilterXObject **assoc_object)
{
  RepeatedPtrField<KeyValue> *repeated_kv = _get_repeated_kv(message, reflectors);
  if (!filterx_dict_iter(object, _add_elem_to_repeated_kv, repeated_kv))
    return false;

  *assoc_object = _new_borrowed(repeated_kv);
  return true;
}

bool
OtelKVListField::FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors,
                                     FilterXObject *object, FilterXObject **assoc_object)
{
  if (!filterx_object_is_type(object, &FILTERX_TYPE_NAME(otel_kvlist)))
    {
      if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(dict)))
        return _set_kvlist_field_from_dict(message, reflectors, object, assoc_object);

      msg_error("otel-kvlist: Failed to convert field, type is unsupported",
                evt_tag_str("field", reflectors.fieldDescriptor->name().c_str()),
                evt_tag_str("expected_type", reflectors.fieldDescriptor->type_name()),
                evt_tag_str("type", object->type->name));
      return false;
    }

  FilterXOtelKVList *filterx_kvlist = (FilterXOtelKVList *) object;

  RepeatedPtrField<KeyValue> *repeated_kv = _get_repeated_kv(message, reflectors);
  repeated_kv->CopyFrom(filterx_kvlist->cpp->get_value());

  KVList *new_kvlist;
  try
    {
      new_kvlist = new KVList(filterx_kvlist, repeated_kv);
    }
  catch (const std::runtime_error &)
    {
      g_assert_not_reached();
    }

  delete filterx_kvlist->cpp;
  filterx_kvlist->cpp = new_kvlist;

  return true;
}

OtelKVListField syslogng::grpc::otel::filterx::otel_kvlist_converter;

FILTERX_DEFINE_TYPE(otel_kvlist, FILTERX_TYPE_NAME(dict),
                    .is_mutable = TRUE,
                    .marshal = _marshal,
                    .clone = _filterx_otel_kvlist_clone,
                    .truthy = _truthy,
                    .list_factory = filterx_otel_array_new,
                    .dict_factory = filterx_otel_kvlist_new,
                    .free_fn = _free,
                   );
