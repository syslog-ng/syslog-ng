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

#include "object-otel-logrecord.hpp"
#include "protobuf-field.hpp"
#include "otel-field.hpp"

#include "compat/cpp-start.h"

#include "filterx/object-string.h"
#include "filterx/object-datetime.h"
#include "filterx/object-primitive.h"
#include "scratch-buffers.h"
#include "generic-number.h"

#include "compat/cpp-end.h"

#include <unistd.h>
#include <cstdio>
#include <memory>
#include <stdexcept>

using namespace syslogng::grpc::otel::filterx;

/* C++ Implementations */

LogRecord::LogRecord(FilterXOtelLogRecord *super_) : super(super_)
{
}

LogRecord::LogRecord(FilterXOtelLogRecord *super_, FilterXObject *protobuf_object) : super(super_)
{
  gsize length;
  const gchar *value = filterx_protobuf_get_value(protobuf_object, &length);

  if (!value)
    throw std::runtime_error("Argument is not a protobuf object");

  if (!logRecord.ParsePartialFromArray(value, length))
    throw std::runtime_error("Failed to parse from protobuf object");
}

LogRecord::LogRecord(const LogRecord &o, FilterXOtelLogRecord *super_) : super(super_),
  logRecord(o.logRecord)
{
}

std::string
LogRecord::Marshal(void)
{
  std::string serializedString = this->logRecord.SerializePartialAsString();
  return serializedString;
}

bool
LogRecord::SetField(const gchar *attribute, FilterXObject **value)
{
  try
    {
      ProtoReflectors reflectors(this->logRecord, attribute);
      FilterXObject *assoc_object = NULL;
      if (!otel_converter_by_field_descriptor(reflectors.fieldDescriptor)->Set(&this->logRecord, attribute, *value,
          &assoc_object))
        return false;

      filterx_object_unref(*value);
      *value = assoc_object;
      return true;
    }
  catch(const std::exception &ex)
    {
      msg_error("protobuf-field: Failed to set field:",
                evt_tag_str("message", ex.what()),
                evt_tag_str("field_name", attribute));
      return false;
    }
}

FilterXObject *
LogRecord::GetField(const gchar *attribute)
{
  try
    {
      ProtoReflectors reflectors(this->logRecord, attribute);
      return otel_converter_by_field_descriptor(reflectors.fieldDescriptor)->Get(&this->logRecord, attribute);
    }
  catch(const std::exception &ex)
    {
      msg_error("protobuf-field: Failed to get field:",
                evt_tag_str("message", ex.what()),
                evt_tag_str("field_name", attribute));
      return nullptr;
    }
}

const opentelemetry::proto::logs::v1::LogRecord &
LogRecord::GetValue() const
{
  return this->logRecord;
}

/* C Wrappers */

gpointer
grpc_otel_filterx_logrecord_contruct_new(Plugin *self)
{
  return (gpointer) &filterx_otel_logrecord_new_from_args;
}

FilterXObject *
_filterx_otel_logrecord_clone(FilterXObject *s)
{
  FilterXOtelLogRecord *self = (FilterXOtelLogRecord *) s;

  FilterXOtelLogRecord *clone = g_new0(FilterXOtelLogRecord, 1);
  filterx_object_init_instance((FilterXObject *) clone, &FILTERX_TYPE_NAME(otel_logrecord));

  try
    {
      clone->cpp = new LogRecord(*self->cpp, clone);
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
  FilterXOtelLogRecord *self = (FilterXOtelLogRecord *) s;

  delete self->cpp;
  self->cpp = NULL;
}

static gboolean
_setattr(FilterXObject *s, FilterXObject *attr, FilterXObject **new_value)
{
  FilterXOtelLogRecord *self = (FilterXOtelLogRecord *) s;

  return self->cpp->SetField(filterx_string_get_value(attr, NULL), new_value);
}

static FilterXObject *
_getattr(FilterXObject *s, FilterXObject *attr)
{
  FilterXOtelLogRecord *self = (FilterXOtelLogRecord *) s;

  return self->cpp->GetField(filterx_string_get_value(attr, NULL));
}

static gboolean
_truthy(FilterXObject *s)
{
  return TRUE;
}

static gboolean
_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXOtelLogRecord *self = (FilterXOtelLogRecord *) s;

  std::string serialized = self->cpp->Marshal();

  g_string_truncate(repr, 0);
  g_string_append_len(repr, serialized.c_str(), serialized.length());
  *t = LM_VT_PROTOBUF;
  return TRUE;
}

FilterXObject *
filterx_otel_logrecord_new_from_args(GPtrArray *args)
{
  FilterXOtelLogRecord *self = g_new0(FilterXOtelLogRecord, 1);
  filterx_object_init_instance((FilterXObject *) self, &FILTERX_TYPE_NAME(otel_logrecord));

  try
    {
      if (!args || args->len == 0)
        self->cpp = new LogRecord(self);
      else if (args->len == 1)
        self->cpp = new LogRecord(self, (FilterXObject *) g_ptr_array_index(args, 0));
      else
        throw std::runtime_error("Invalid number of arguments");
    }
  catch (const std::runtime_error &e)
    {
      msg_error("FilterX: Failed to create OTel LogRecord object", evt_tag_str("error", e.what()));
      filterx_object_unref(&self->super);
      return NULL;
    }

  return &self->super;
}

FILTERX_DEFINE_TYPE(otel_logrecord, FILTERX_TYPE_NAME(object),
                    .is_mutable = TRUE,
                    .marshal = _marshal,
                    .clone = _filterx_otel_logrecord_clone,
                    .truthy = _truthy,
                    .getattr = _getattr,
                    .setattr = _setattr,
                    .free_fn = _free,
                   );
