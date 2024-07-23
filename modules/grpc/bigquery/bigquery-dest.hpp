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

#ifndef BIGQUERY_DEST_HPP
#define BIGQUERY_DEST_HPP

#include "bigquery-dest.h"

#include "compat/cpp-start.h"
#include "template/templates.h"
#include "stats/stats-cluster-key-builder.h"
#include "compat/cpp-end.h"

#include "metrics/grpc-metrics.hpp"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/message.h>
#include <google/protobuf/compiler/importer.h>

#include <string>
#include <memory>
#include <vector>

namespace syslogng {
namespace grpc {
namespace bigquery {

struct Field
{
  std::string name;
  google::protobuf::FieldDescriptorProto::Type type;
  LogTemplate *value;
  const google::protobuf::FieldDescriptor *field_desc;

  Field(std::string name_, google::protobuf::FieldDescriptorProto::Type type_, LogTemplate *value_)
    : name(name_), type(type_), value(log_template_ref(value_)), field_desc(nullptr) {}

  Field(const Field &a)
    : name(a.name), type(a.type), value(log_template_ref(a.value)), field_desc(a.field_desc) {}

  Field &operator=(const Field &a)
  {
    name = a.name;
    type = a.type;
    log_template_unref(value);
    value = log_template_ref(a.value);
    field_desc = a.field_desc;

    return *this;
  }

  ~Field()
  {
    log_template_unref(value);
  }

};

class DestinationDriver final
{
public:
  DestinationDriver(BigQueryDestDriver *s);
  ~DestinationDriver();
  bool init();
  bool deinit();
  const gchar *format_persist_name();
  const gchar *format_stats_key(StatsClusterKeyBuilder *kb);

  bool add_field(std::string name, std::string type, LogTemplate *value);
  void set_protobuf_schema(std::string proto_path, GList *values);

  LogTemplateOptions &get_template_options()
  {
    return this->template_options;
  }

  void set_url(std::string u)
  {
    this->url = u;
  }

  void set_project(std::string p)
  {
    this->project = p;
  }

  void set_dataset(std::string d)
  {
    this->dataset = d;
  }

  void set_table(std::string t)
  {
    this->table = t;
  }

  void set_batch_bytes(size_t b)
  {
    this->batch_bytes = b;
  }

  void set_compression(bool b)
  {
    this->compression = b;
  }

  void set_keepalive_time(int t)
  {
    this->keepalive_time = t;
  }

  void set_keepalive_timeout(int t)
  {
    this->keepalive_timeout = t;
  }

  void set_keepalive_max_pings(int p)
  {
    this->keepalive_max_pings_without_data = p;
  }

  void add_extra_channel_arg(std::string name, long value)
  {
    this->int_extra_channel_args.push_back(std::pair<std::string, long> {name, value});
  }

  void add_extra_channel_arg(std::string name, std::string value)
  {
    this->string_extra_channel_args.push_back(std::pair<std::string, std::string> {name, value});
  }

  void add_header(std::string name, std::string value)
  {
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    this->headers.push_back(std::pair<std::string, std::string> {name, value});
  }

  const std::string &get_url()
  {
    return this->url;
  }

  const std::string &get_project()
  {
    return this->project;
  }

  const std::string &get_dataset()
  {
    return this->dataset;
  }

  const std::string &get_table()
  {
    return this->table;
  }

private:
  friend class DestinationWorker;
  void construct_schema_prototype();
  bool load_protobuf_schema();

private:
  BigQueryDestDriver *super;
  LogTemplateOptions template_options;

  std::string url;
  std::string project;
  std::string dataset;
  std::string table;

  size_t batch_bytes;

  int keepalive_time;
  int keepalive_timeout;
  int keepalive_max_pings_without_data;
  bool compression;

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

  std::list<std::pair<std::string, long>> int_extra_channel_args;
  std::list<std::pair<std::string, std::string>> string_extra_channel_args;
  std::list<std::pair<std::string, std::string>> headers;

  DestDriverMetrics metrics;
};


}
}
}

syslogng::grpc::bigquery::DestinationDriver *bigquery_dd_get_cpp(BigQueryDestDriver *self);

#endif
