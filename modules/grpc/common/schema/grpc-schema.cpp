/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
 * Copyright (c) 2023 László Várady
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

#include "grpc-schema.hpp"

#include "compat/cpp-start.h"
#include "scratch-buffers.h"
#include "compat/cpp-end.h"

#include <absl/strings/string_view.h>

using namespace syslogng::grpc;

static void
_template_unref(gpointer data)
{
  LogTemplate *tpl = (LogTemplate *) data;
  log_template_unref(tpl);
}

namespace {
class ErrorCollector : public google::protobuf::compiler::MultiFileErrorCollector
{
public:
  ErrorCollector() {}
  ~ErrorCollector() override {}

  // override is missing for compatibility with older protobuf versions
  void RecordError(absl::string_view filename, int line, int column, absl::string_view message)
  {
    std::string file{filename};
    std::string msg{message};

    msg_error("Error parsing protobuf-schema() file",
              evt_tag_str("filename", file.c_str()), evt_tag_int("line", line), evt_tag_int("column", column),
              evt_tag_str("error", msg.c_str()));
  }

  // override is missing for compatibility with older protobuf versions
  void RecordWarning(absl::string_view filename, int line, int column, absl::string_view message)
  {
    std::string file{filename};
    std::string msg{message};

    msg_error("Warning during parsing protobuf-schema() file",
              evt_tag_str("filename", file.c_str()), evt_tag_int("line", line), evt_tag_int("column", column),
              evt_tag_str("warning", msg.c_str()));
  }

private:
  /* deprecated interface */
  void AddError(const std::string &filename, int line, int column, const std::string &message)
  {
    this->RecordError(filename, line, column, message);
  }

  void AddWarning(const std::string &filename, int line, int column, const std::string &message)
  {
    this->RecordWarning(filename, line, column, message);
  }
};
}

Schema::Schema(int proto_version,
               const std::string &file_descriptor_proto_name_,
               const std::string &descriptor_proto_name_,
               MapTypeFn map_type_,
               LogTemplateOptions *template_options_,
               LogPipe *log_pipe_) :
  log_pipe(log_pipe_),
  map_type(map_type_),
  template_options(template_options_),
  syntax("proto" + std::to_string(proto_version)),
  file_descriptor_proto_name(file_descriptor_proto_name_),
  descriptor_proto_name(descriptor_proto_name_)
{
}

Schema::~Schema()
{
  g_list_free_full(this->protobuf_schema.values, _template_unref);
}

bool
Schema::init()
{
  if (!this->protobuf_schema.proto_path.empty())
    return this->protobuf_schema.loaded || this->load_protobuf_schema();

  this->construct_schema_prototype();
  return true;
}

void
Schema::construct_schema_prototype()
{
  this->msg_factory = std::make_unique<google::protobuf::DynamicMessageFactory>();
  this->descriptor_pool.~DescriptorPool();
  new (&this->descriptor_pool) google::protobuf::DescriptorPool();

  google::protobuf::FileDescriptorProto file_descriptor_proto;
  file_descriptor_proto.set_name(this->file_descriptor_proto_name);
  file_descriptor_proto.set_syntax(this->syntax);
  google::protobuf::DescriptorProto *descriptor_proto = file_descriptor_proto.add_message_type();
  descriptor_proto->set_name(this->descriptor_proto_name);

  int32_t num = 1;
  for (auto &field : this->fields)
    {
      google::protobuf::FieldDescriptorProto *field_desc_proto = descriptor_proto->add_field();
      field_desc_proto->set_name(field.nv.name);
      field_desc_proto->set_type(field.type);
      field_desc_proto->set_number(num++);
    }

  const google::protobuf::FileDescriptor *file_descriptor = this->descriptor_pool.BuildFile(file_descriptor_proto);
  this->schema_descriptor = file_descriptor->message_type(0);

  for (int i = 0; i < this->schema_descriptor->field_count(); ++i)
    {
      this->fields[i].field_desc = this->schema_descriptor->field(i);
    }

  this->schema_prototype = this->msg_factory->GetPrototype(this->schema_descriptor);
}

bool
Schema::load_protobuf_schema()
{
  this->protobuf_schema.loaded = false;
  this->msg_factory = std::make_unique<google::protobuf::DynamicMessageFactory>();
  this->protobuf_schema.importer.reset(nullptr);

  this->protobuf_schema.src_tree = std::make_unique<google::protobuf::compiler::DiskSourceTree>();
  this->protobuf_schema.src_tree->MapPath(this->protobuf_schema.proto_path, this->protobuf_schema.proto_path);

  this->protobuf_schema.error_coll = std::make_unique<ErrorCollector>();

  this->protobuf_schema.importer =
    std::make_unique<google::protobuf::compiler::Importer>(this->protobuf_schema.src_tree.get(),
                                                           this->protobuf_schema.error_coll.get());

  const google::protobuf::FileDescriptor *file_descriptor =
    this->protobuf_schema.importer->Import(this->protobuf_schema.proto_path);

  if (!file_descriptor || file_descriptor->message_type_count() == 0)
    {
      msg_error("Error initializing gRPC based destination, protobuf-schema() file can't be loaded",
                log_pipe_location_tag(this->log_pipe));
      return false;
    }

  this->schema_descriptor = file_descriptor->message_type(0);

  this->fields.clear();

  GList *current_value = this->protobuf_schema.values;
  for (int i = 0; i < this->schema_descriptor->field_count(); ++i)
    {
      auto field = this->schema_descriptor->field(i);

      if (!current_value)
        {
          msg_error("Error initializing gRPC based destination, protobuf-schema() file has more fields than "
                    "values listed in the config",
                    log_pipe_location_tag(this->log_pipe));
          return false;
        }

      LogTemplate *value = (LogTemplate *) current_value->data;

      this->fields.push_back(Field{field->name(), (google::protobuf::FieldDescriptorProto::Type) field->type(), value});
      this->fields[i].field_desc = field;

      current_value = current_value->next;
    }

  if (current_value)
    {
      msg_error("Error initializing gRPC based destination, protobuf-schema() file has less fields than "
                "values listed in the config",
                log_pipe_location_tag(this->log_pipe));
      return false;
    }


  this->schema_prototype = this->msg_factory->GetPrototype(this->schema_descriptor);
  this->protobuf_schema.loaded = true;
  return true;
}

bool
Schema::add_field(std::string name, std::string type, LogTemplate *value)
{
  google::protobuf::FieldDescriptorProto::Type proto_type;
  if (!this->map_type(type, proto_type))
    return false;

  this->fields.push_back(Field{name, proto_type, value});
  return true;
}

void
Schema::set_protobuf_schema(std::string proto_path, GList *values)
{
  this->protobuf_schema.proto_path = proto_path;

  g_list_free_full(this->protobuf_schema.values, _template_unref);
  this->protobuf_schema.values = values;
}

google::protobuf::Message *
Schema::format(LogMessage *msg, gint seq_num) const
{
  google::protobuf::Message *message = schema_prototype->New();
  const google::protobuf::Reflection *reflection = message->GetReflection();

  bool msg_has_field = false;
  for (const auto &field : fields)
    {
      bool field_inserted = this->insert_field(reflection, field, seq_num, msg, message);
      msg_has_field |= field_inserted;

      if (!field_inserted && (this->template_options->on_error & ON_ERROR_DROP_MESSAGE))
        goto drop;
    }

  if (!msg_has_field)
    goto drop;

  return message;

drop:
  delete message;
  return nullptr;
}

Schema::Slice
Schema::format_template(LogTemplate *tmpl, LogMessage *msg, GString *value, LogMessageValueType *type,
                        gint seq_num) const
{
  if (log_template_is_trivial(tmpl))
    {
      gssize trivial_value_len;
      const gchar *trivial_value = log_template_get_trivial_value_and_type(tmpl, msg, &trivial_value_len, type);

      if (trivial_value_len < 0)
        return Slice{"", 0};

      return Slice{trivial_value, (std::size_t) trivial_value_len};
    }

  LogTemplateEvalOptions options = {this->template_options, LTZ_SEND, seq_num, NULL, LM_VT_STRING};
  log_template_format_value_and_type(tmpl, msg, &options, value, type);
  return Slice{value->str, value->len};
}

bool
Schema::insert_field(const google::protobuf::Reflection *reflection, const Field &field, gint seq_num,
                     LogMessage *msg, google::protobuf::Message *message) const
{
  ScratchBuffersMarker m;
  GString *buf = scratch_buffers_alloc_and_mark(&m);

  LogMessageValueType type;

  Slice value = this->format_template(field.nv.value, msg, buf, &type, seq_num);

  if (type == LM_VT_NULL)
    {
      if (field.field_desc->is_required())
        {
          msg_error("Missing required field", evt_tag_str("field", field.nv.name.c_str()));
          goto error;
        }

      scratch_buffers_reclaim_marked(m);
      return true;
    }

  switch (field.field_desc->cpp_type())
    {
    /* TYPE_STRING, TYPE_BYTES (embedded nulls are possible, no null-termination is assumed) */
    case google::protobuf::FieldDescriptor::CppType::CPPTYPE_STRING:
      reflection->SetString(message, field.field_desc, std::string{value.str, value.len});
      break;
    case google::protobuf::FieldDescriptor::CppType::CPPTYPE_INT32:
    {
      int32_t v;
      if (!type_cast_to_int32(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(this->template_options->on_error, value.str, -1, "integer");
          goto error;
        }
      reflection->SetInt32(message, field.field_desc, v);
      break;
    }
    case google::protobuf::FieldDescriptor::CppType::CPPTYPE_INT64:
    {
      gint64 v;
      if (!type_cast_to_int64(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(this->template_options->on_error, value.str, -1, "integer");
          goto error;
        }
      reflection->SetInt64(message, field.field_desc, v);
      break;
    }
    case google::protobuf::FieldDescriptor::CppType::CPPTYPE_UINT32:
    {
      gint64 v;
      if (!type_cast_to_int64(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(this->template_options->on_error, value.str, -1, "integer");
          goto error;
        }
      reflection->SetUInt32(message, field.field_desc, (uint32_t) v);
      break;
    }
    case google::protobuf::FieldDescriptor::CppType::CPPTYPE_UINT64:
    {
      gint64 v;
      if (!type_cast_to_int64(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(this->template_options->on_error, value.str, -1, "integer");
          goto error;
        }
      reflection->SetUInt64(message, field.field_desc, (uint64_t) v);
      break;
    }
    case google::protobuf::FieldDescriptor::CppType::CPPTYPE_DOUBLE:
    {
      double v;
      if (!type_cast_to_double(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(this->template_options->on_error, value.str, -1, "double");
          goto error;
        }
      reflection->SetDouble(message, field.field_desc, v);
      break;
    }
    case google::protobuf::FieldDescriptor::CppType::CPPTYPE_FLOAT:
    {
      double v;
      if (!type_cast_to_double(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(this->template_options->on_error, value.str, -1, "double");
          goto error;
        }
      reflection->SetFloat(message, field.field_desc, (float) v);
      break;
    }
    case google::protobuf::FieldDescriptor::CppType::CPPTYPE_BOOL:
    {
      gboolean v;
      if (!type_cast_to_boolean(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(this->template_options->on_error, value.str, -1, "boolean");
          goto error;
        }
      reflection->SetBool(message, field.field_desc, v);
      break;
    }
    default:
      goto error;
    }

  scratch_buffers_reclaim_marked(m);
  return true;

error:
  scratch_buffers_reclaim_marked(m);
  return false;
}
