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

using namespace syslogng::grpc::otel;

/* C++ Implementations */
class OtelConverter : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(const google::protobuf::Message &message, ProtoReflectors reflectors)
  {
    const std::string &fieldName = reflectors.fieldDescriptor->name();
    if (fieldName.compare("time_unix_nano") == 0 ||
        fieldName.compare("observed_time_unix_nano") == 0)
      {
        uint64_t val = reflectors.reflection->GetUInt64(message, reflectors.fieldDescriptor);
        UnixTime utime = unix_time_from_unix_epoch(val);
        return filterx_datetime_new(&utime);
      }

    ProtobufField *converter = protobuf_converter_by_type(reflectors.fieldType);
    if (converter)
      {
        return converter->Get(message, fieldName);
      }
    else
      {
        msg_error("field type not yet implemented", evt_tag_str("name", fieldName.c_str()), evt_tag_int("type",
                  reflectors.fieldType));
      }
    return nullptr;
  }
  bool FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object)
  {
    const std::string &fieldName = reflectors.fieldDescriptor->name();
    if (fieldName.compare("time_unix_nano") == 0 ||
        fieldName.compare("observed_time_unix_nano") == 0)
      {
        if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(datetime)))
          {
            const UnixTime utime = filterx_datetime_get_value(object);
            uint64_t unix_epoch = unix_time_to_unix_epoch(utime);
            reflectors.reflection->SetUInt64(message, reflectors.fieldDescriptor, unix_epoch);
            return true;
          }
      }
    ProtobufField *converter = protobuf_converter_by_type(reflectors.fieldType);
    if (converter)
      {
        return converter->Set(message, fieldName, object);
      }
    else
      {
        msg_error("field type not yet implemented", evt_tag_str("name", fieldName.c_str()), evt_tag_int("type",
                  reflectors.fieldType));
      }
    return false;
  }
};

static OtelConverter otel_converter;

OtelLogRecordCpp::OtelLogRecordCpp(FilterXOtelLogRecord *folr) : super(folr)
{
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

bool OtelLogRecordCpp::SetField(const gchar *attribute, FilterXObject *value)
{
  return otel_converter.Set(&this->logRecord, attribute, value);
}

FilterXObject *
OtelLogRecordCpp::GetField(const gchar *attribute)
{
  return otel_converter.Get(this->logRecord, attribute);
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
  folr->cpp = new OtelLogRecordCpp(*self->cpp, folr);

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
  folr->cpp = new OtelLogRecordCpp(folr);
  return folr->cpp->FilterX();
}

FILTERX_DEFINE_TYPE(olr, FILTERX_TYPE_NAME(object),
                    .is_mutable = TRUE,
                    .marshal = _marshal,
                    .clone = _filterx_otel_logrecord_clone,
                    .truthy = _truthy,
                    .getattr = _getattr,
                    .setattr = _setattr,
                    .free_fn = _free,
                    // .map_to_json = _map_to_json,
                   );
