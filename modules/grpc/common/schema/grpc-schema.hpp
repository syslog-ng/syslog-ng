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

#ifndef GRPC_SCHEMA_HPP
#define GRPC_SCHEMA_HPP

#include "syslog-ng.h"

#include "compat/cpp-start.h"
#include "template/templates.h"
#include "logpipe.h"
#include "compat/cpp-end.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/message.h>
#include <google/protobuf/compiler/importer.h>
#include <grpc++/grpc++.h>

#include <memory>
#include <vector>

namespace syslogng {
namespace grpc {

struct NameValueTemplatePair
{
  std::string name;
  LogTemplate *value;

  NameValueTemplatePair(std::string name_, LogTemplate *value_)
    : name(name_), value(log_template_ref(value_)) {}

  NameValueTemplatePair(const NameValueTemplatePair &a)
    : name(a.name), value(log_template_ref(a.value)) {}

  NameValueTemplatePair &operator=(const NameValueTemplatePair &a)
  {
    name = a.name;
    log_template_unref(value);
    value = log_template_ref(a.value);

    return *this;
  }

  ~NameValueTemplatePair()
  {
    log_template_unref(value);
  }

};

struct Field
{
  NameValueTemplatePair nv;
  google::protobuf::FieldDescriptorProto::Type type;
  const google::protobuf::FieldDescriptor *field_desc;

  Field(std::string name_, google::protobuf::FieldDescriptorProto::Type type_, LogTemplate *value_)
    : nv(name_, value_), type(type_), field_desc(nullptr) {}

  Field(const Field &a)
    : nv(a.nv), type(a.type), field_desc(a.field_desc) {}

  Field &operator=(const Field &a)
  {
    nv = a.nv;
    type = a.type;
    field_desc = a.field_desc;

    return *this;
  }

};

class Schema
{
private:
  struct Slice
  {
    const char *str;
    std::size_t len;
  };

public:
  using MapTypeFn =
    std::function<bool (const std::string &type_in, google::protobuf::FieldDescriptorProto::Type &type_out)>;

public:
  Schema(int proto_version, const std::string &file_descriptor_proto_name, const std::string &descriptor_proto_name,
         MapTypeFn map_type, LogTemplateOptions *template_options, LogPipe *log_pipe);
  ~Schema();

  bool init();
  google::protobuf::Message *format(LogMessage *msg, gint seq_num) const;

  bool empty() const
  {
    return this->fields.empty();
  }

  const google::protobuf::Descriptor &get_schema_descriptor() const
  {
    return *this->schema_descriptor;
  }

  /* For grammar. */
  bool add_field(std::string name, std::string type, LogTemplate *value);
  void set_protobuf_schema(std::string proto_path, GList *values);

private:
  void construct_schema_prototype();
  bool load_protobuf_schema();
  Slice format_template(LogTemplate *tmpl, LogMessage *msg, GString *value, LogMessageValueType *type,
                        gint seq_num) const;
  bool insert_field(const google::protobuf::Reflection *reflection, const Field &field, gint seq_num,
                    LogMessage *msg, google::protobuf::Message *message) const;

private:
  LogPipe *log_pipe;
  MapTypeFn map_type;
  LogTemplateOptions *template_options;

  std::string syntax;
  std::string file_descriptor_proto_name;
  std::string descriptor_proto_name;

  struct
  {
    std::string proto_path;
    GList *values = nullptr;

    std::unique_ptr<google::protobuf::compiler::DiskSourceTree> src_tree;
    std::unique_ptr<google::protobuf::compiler::MultiFileErrorCollector> error_coll;
    std::unique_ptr<google::protobuf::compiler::Importer> importer;
    bool loaded = false;
  } protobuf_schema;

  std::vector<Field> fields;

  google::protobuf::DescriptorPool descriptor_pool;

  /* A given descriptor_pool/importer instance should outlive msg_factory, as msg_factory caches prototypes */
  std::unique_ptr<google::protobuf::DynamicMessageFactory> msg_factory;
  const google::protobuf::Descriptor *schema_descriptor = nullptr;
  const google::protobuf::Message *schema_prototype  = nullptr;
};

}
}

#endif
