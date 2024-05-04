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

#include "object-otel-scope.hpp"
#include "otel-field.hpp"

#include "compat/cpp-start.h"
#include "filterx/object-string.h"
#include "compat/cpp-end.h"

#include <stdexcept>

using namespace syslogng::grpc::otel::filterx;

/* C++ Implementations */

Scope::Scope(FilterXOtelScope *s) : super(s)
{
}

Scope::Scope(FilterXOtelScope *s, FilterXObject *protobuf_object) : super(s)
{
  gsize length;
  const gchar *value = filterx_protobuf_get_value(protobuf_object, &length);

  if (!value)
    throw std::runtime_error("Argument is not a protobuf object");

  if (!scope.ParsePartialFromArray(value, length))
    throw std::runtime_error("Failed to parse from protobuf object");
}

Scope::Scope(const Scope &o, FilterXOtelScope *s) : super(s), scope(o.scope)
{
}

std::string
Scope::marshal(void)
{
  return scope.SerializePartialAsString();
}

bool
Scope::set_field(const gchar *attribute, FilterXObject **value)
{
  try
    {
      ProtoReflectors reflectors(scope, attribute);
      FilterXObject *assoc_object = NULL;
      if (!otel_converter_by_field_descriptor(reflectors.fieldDescriptor)->Set(&scope, attribute, *value, &assoc_object))
        return false;

      filterx_object_unref(*value);
      *value = assoc_object;
      return true;
    }
  catch (const std::invalid_argument &e)
    {
      msg_error("FilterX: Failed to set OTel Scope field",
                evt_tag_str("field_name", attribute),
                evt_tag_str("error", e.what()));
      return false;
    }
}

FilterXObject *
Scope::get_field(const gchar *attribute)
{
  try
    {
      ProtoReflectors reflectors(scope, attribute);
      return otel_converter_by_field_descriptor(reflectors.fieldDescriptor)->Get(&scope, attribute);
    }
  catch (const std::invalid_argument &e)
    {
      msg_error("FilterX: Failed to get OTel Scope field",
                evt_tag_str("field_name", attribute),
                evt_tag_str("error", e.what()));
      return nullptr;
    }
}

const opentelemetry::proto::common::v1::InstrumentationScope &
Scope::get_value() const
{
  return scope;
}

/* C Wrappers */

FilterXObject *
_filterx_otel_scope_clone(FilterXObject *s)
{
  FilterXOtelScope *self = (FilterXOtelScope *) s;

  FilterXOtelScope *clone = g_new0(FilterXOtelScope, 1);
  filterx_object_init_instance(&clone->super, &FILTERX_TYPE_NAME(otel_scope));

  try
    {
      clone->cpp = new Scope(*self->cpp, self);
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
  FilterXOtelScope *self = (FilterXOtelScope *) s;

  delete self->cpp;
  self->cpp = NULL;
}

static gboolean
_setattr(FilterXObject *s, FilterXObject *attr, FilterXObject **new_value)
{
  FilterXOtelScope *self = (FilterXOtelScope *) s;

  return self->cpp->set_field(filterx_string_get_value(attr, NULL), new_value);
}

static FilterXObject *
_getattr(FilterXObject *s, FilterXObject *attr)
{
  FilterXOtelScope *self = (FilterXOtelScope *) s;

  return self->cpp->get_field(filterx_string_get_value(attr, NULL));
}

static gboolean
_truthy(FilterXObject *s)
{
  return TRUE;
}

static gboolean
_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXOtelScope *self = (FilterXOtelScope *) s;

  std::string serialized = self->cpp->marshal();

  g_string_truncate(repr, 0);
  g_string_append_len(repr, serialized.c_str(), serialized.length());
  *t = LM_VT_PROTOBUF;
  return TRUE;
}

FilterXObject *
filterx_otel_scope_new_from_args(GPtrArray *args)
{
  FilterXOtelScope *s = g_new0(FilterXOtelScope, 1);
  filterx_object_init_instance((FilterXObject *)s, &FILTERX_TYPE_NAME(otel_scope));

  try
    {
      if (!args || args->len == 0)
        s->cpp = new Scope(s);
      else if (args->len == 1)
        s->cpp = new Scope(s, (FilterXObject *) g_ptr_array_index(args, 0));
      else
        throw std::runtime_error("Invalid number of arguments");
    }
  catch (const std::runtime_error &e)
    {
      msg_error("FilterX: Failed to create OTel Scope object", evt_tag_str("error", e.what()));
      filterx_object_unref(&s->super);
      return NULL;
    }

  return &s->super;
}

gpointer
grpc_otel_filterx_scope_construct_new(Plugin *self)
{
  return (gpointer) &filterx_otel_scope_new_from_args;
}

FILTERX_DEFINE_TYPE(otel_scope, FILTERX_TYPE_NAME(object),
                    .is_mutable = TRUE,
                    .marshal = _marshal,
                    .clone = _filterx_otel_scope_clone,
                    .truthy = _truthy,
                    .getattr = _getattr,
                    .setattr = _setattr,
                    .free_fn = _free,
                   );
