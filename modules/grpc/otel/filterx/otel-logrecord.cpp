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

#include "otel-logrecord.hpp"
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

using namespace syslogng::grpc::otel;

/* C++ Implementations */

OtelLogRecordCpp::OtelLogRecordCpp(FilterXOtelLogRecord *folr) : super(folr)
{
}

OtelLogRecordCpp::OtelLogRecordCpp(FilterXOtelLogRecord *folr, FilterXObject *protobuf_object) : super(folr)
{
  gsize length;
  const gchar *value = filterx_protobuf_get_value(protobuf_object, &length);

  if (!value)
    throw std::runtime_error("Argument is not a protobuf object");

  if (!logRecord.ParsePartialFromArray(value, length))
    throw std::runtime_error("Failed to parse from protobuf object");
}

OtelLogRecordCpp::OtelLogRecordCpp(const OtelLogRecordCpp &o, FilterXOtelLogRecord *folr) : super(folr),
  logRecord(o.logRecord)
{
}

FilterXObject *OtelLogRecordCpp::FilterX()
{
  return (FilterXObject *)this->super;
}

std::string
OtelLogRecordCpp::Marshal(void)
{
  std::string serializedString = this->logRecord.SerializePartialAsString();
  return serializedString;
}

bool
OtelLogRecordCpp::SetField(const gchar *attribute, FilterXObject *value)
{
  try
    {
      ProtoReflectors reflectors(this->logRecord, attribute);
      return otel_converter_by_field_descriptor(reflectors.fieldDescriptor)->Set(&this->logRecord, attribute, value);
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
OtelLogRecordCpp::GetField(const gchar *attribute)
{
  try
    {
      ProtoReflectors reflectors(this->logRecord, attribute);
      return otel_converter_by_field_descriptor(reflectors.fieldDescriptor)->Get(this->logRecord, attribute);
    }
  catch(const std::exception &ex)
    {
      msg_error("protobuf-field: Failed to get field:",
                evt_tag_str("message", ex.what()),
                evt_tag_str("field_name", attribute));
      return nullptr;
    }
}

const LogRecord &
OtelLogRecordCpp::GetValue() const
{
  return this->logRecord;
}

/* C Wrappers */

gpointer
grpc_otel_filterx_logrecord_contruct_new(Plugin *self)
{
  return (gpointer) &otel_logrecord;
}

FilterXObject *
_filterx_otel_logrecord_clone(FilterXObject *s)
{
  FilterXOtelLogRecord *self = (FilterXOtelLogRecord *) s;

  FilterXOtelLogRecord *folr = g_new0(FilterXOtelLogRecord, 1);
  filterx_object_init_instance((FilterXObject *)folr, &FILTERX_TYPE_NAME(olr));
  try
    {
      folr->cpp = new OtelLogRecordCpp(*self->cpp, folr);
    }
  catch (const std::runtime_error &)
    {
      g_assert_not_reached();
    }

  return folr->cpp->FilterX();
}

static void
_free(FilterXObject *s)
{
  FilterXOtelLogRecord *self = (FilterXOtelLogRecord *) s;

  delete self->cpp;
  self->cpp = NULL;
}

static gboolean
_setattr(FilterXObject *s, const gchar *attr_name, FilterXObject *new_value)
{
  FilterXOtelLogRecord *self = (FilterXOtelLogRecord *) s;

  return self->cpp->SetField(attr_name, new_value);
}

static FilterXObject *
_getattr(FilterXObject *s, const gchar *attr_name)
{
  FilterXOtelLogRecord *self = (FilterXOtelLogRecord *) s;

  return self->cpp->GetField(attr_name);
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
otel_logrecord(GPtrArray *args)
{
  FilterXOtelLogRecord *folr = g_new0(FilterXOtelLogRecord, 1);
  filterx_object_init_instance((FilterXObject *)folr, &FILTERX_TYPE_NAME(olr));

  try
    {
      if (!args || args->len == 0)
        folr->cpp = new OtelLogRecordCpp(folr);
      else if (args->len == 1)
        folr->cpp = new OtelLogRecordCpp(folr, (FilterXObject *) g_ptr_array_index(args, 0));
      else
        throw std::runtime_error("Invalid number of arguments");
    }
  catch (const std::runtime_error &e)
    {
      msg_error("FilterX: Failed to create OTel LogRecord object", evt_tag_str("error", e.what()));
      filterx_object_unref(&folr->super);
      return NULL;
    }

  return folr->cpp->FilterX();
}

FILTERX_DEFINE_TYPE(olr, FILTERX_TYPE_NAME(object),
                    .is_mutable = TRUE,
                    .marshal = _marshal,
                    .clone = _filterx_otel_logrecord_clone,
                    // .map_to_json = _map_to_json,
                    .truthy = _truthy,
                    .getattr = _getattr,
                    .setattr = _setattr,
                    .free_fn = _free,
                   );
