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

#include "bigquery-dest.hpp"
#include "bigquery-worker.hpp"
#include "grpc-dest-worker.hpp"

#include "compat/cpp-start.h"
#include "logthrdest/logthrdestdrv.h"
#include "messages.h"
#include "compat/cpp-end.h"

#include <absl/strings/string_view.h>

#include <cstring>

using syslogng::grpc::bigquery::DestinationDriver;

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
#if (defined(__clang__) && __clang_major__ >= 10)
#pragma GCC diagnostic ignored "-Winconsistent-missing-override"
#endif
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

DestinationDriver::DestinationDriver(GrpcDestDriver *s)
  : syslogng::grpc::DestDriver(s)
{
  this->url = "bigquerystorage.googleapis.com";
  this->credentials_builder.set_mode(GCAM_ADC);
}

DestinationDriver::~DestinationDriver()
{
  g_list_free_full(this->protobuf_schema.values, _template_unref);
}

bool
DestinationDriver::add_field(std::string name, std::string type, LogTemplate *value)
{
  /* https://cloud.google.com/bigquery/docs/write-api#data_type_conversions */

  google::protobuf::FieldDescriptorProto::Type proto_type;
  const char *type_str = type.c_str();
  if (type.empty() || strcasecmp(type_str, "STRING") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else if (strcasecmp(type_str, "BYTES") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_BYTES;
  else if (strcasecmp(type_str, "INTEGER") == 0 || strcasecmp(type_str, "INT64") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_INT64;
  else if (strcasecmp(type_str, "FLOAT") == 0 || strcasecmp(type_str, "FLOAT64") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_DOUBLE;
  else if (strcasecmp(type_str, "BOOLEAN") == 0 || strcasecmp(type_str, "BOOL") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_BOOL;
  else if (strcasecmp(type_str, "TIMESTAMP") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_INT64;
  else if (strcasecmp(type_str, "DATE") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_INT32;
  else if (strcasecmp(type_str, "TIME") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else if (strcasecmp(type_str, "DATETIME") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else if (strcasecmp(type_str, "JSON") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else if (strcasecmp(type_str, "NUMERIC") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_INT64;
  else if (strcasecmp(type_str, "BIGNUMERIC") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else if (strcasecmp(type_str, "GEOGRAPHY") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else if (strcasecmp(type_str, "RECORD") == 0 || strcasecmp(type_str, "STRUCT") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_MESSAGE;
  else if (strcasecmp(type_str, "INTERVAL") == 0)
    proto_type = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else
    return false;

  this->fields.push_back(Field{name, proto_type, value});

  return true;
}

void
DestinationDriver::set_protobuf_schema(std::string proto_path, GList *values)
{
  this->protobuf_schema.proto_path = proto_path;

  g_list_free_full(this->protobuf_schema.values, _template_unref);
  this->protobuf_schema.values = values;
}

bool
DestinationDriver::init()
{
  if (this->batch_bytes > 10 * 1000 * 1000)
    {
      msg_error("Error initializing BigQuery destination, batch-bytes() cannot be larger than 10 MB. "
                "For more info see https://cloud.google.com/bigquery/quotas#write-api-limits",
                log_pipe_location_tag(&this->super->super.super.super.super));
      return false;
    }

  if (this->protobuf_schema.proto_path.empty())
    this->construct_schema_prototype();
  else
    {
      if (!this->protobuf_schema.loaded && !this->load_protobuf_schema())
        return false;
    }

  if (this->fields.size() == 0)
    {
      msg_error("Error initializing BigQuery destination, schema() or protobuf-schema() is empty",
                log_pipe_location_tag(&this->super->super.super.super.super));
      return false;
    }

  if (this->get_project().empty() || this->get_dataset().empty() || this->get_table().empty())
    {
      msg_error("Error initializing BigQuery destination, project(), dataset(), and table() are mandatory options",
                log_pipe_location_tag(&this->super->super.super.super.super));
      return false;
    }

  return syslogng::grpc::DestDriver::init();
}

const gchar *
DestinationDriver::generate_persist_name()
{
  static gchar persist_name[1024];

  LogPipe *s = &this->super->super.super.super.super;
  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "bigquery.%s", s->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "bigquery(%s,%s,%s,%s)", this->url.c_str(),
               this->project.c_str(), this->dataset.c_str(), this->table.c_str());

  return persist_name;
}

const gchar *
DestinationDriver::format_stats_key(StatsClusterKeyBuilder *kb)
{
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "bigquery"));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("url", this->url.c_str()));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("project", this->project.c_str()));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("dataset", this->dataset.c_str()));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("table", this->table.c_str()));

  return nullptr;
}

LogThreadedDestWorker *
DestinationDriver::construct_worker(int worker_index)
{
  GrpcDestWorker *worker = grpc_dw_new(this->super, worker_index);
  worker->cpp = new DestinationWorker(worker);
  return &worker->super;
}

void
DestinationDriver::construct_schema_prototype()
{
  this->msg_factory = std::make_unique<google::protobuf::DynamicMessageFactory>();
  this->descriptor_pool.~DescriptorPool();
  new (&this->descriptor_pool) google::protobuf::DescriptorPool();

  google::protobuf::FileDescriptorProto file_descriptor_proto;
  file_descriptor_proto.set_name("bigquery_record.proto");
  file_descriptor_proto.set_syntax("proto2");
  google::protobuf::DescriptorProto *descriptor_proto = file_descriptor_proto.add_message_type();
  descriptor_proto->set_name("BigQueryRecord");

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
DestinationDriver::load_protobuf_schema()
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
      msg_error("Error initializing BigQuery destination, protobuf-schema() file can't be loaded",
                log_pipe_location_tag(&this->super->super.super.super.super));
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
          msg_error("Error initializing BigQuery destination, protobuf-schema() file has more fields than "
                    "values listed in the config",
                    log_pipe_location_tag(&this->super->super.super.super.super));
          return false;
        }

      LogTemplate *value = (LogTemplate *) current_value->data;

      this->fields.push_back(Field{field->name(), (google::protobuf::FieldDescriptorProto::Type) field->type(), value});
      this->fields[i].field_desc = field;

      current_value = current_value->next;
    }

  if (current_value)
    {
      msg_error("Error initializing BigQuery destination, protobuf-schema() file has less fields than "
                "values listed in the config",
                log_pipe_location_tag(&this->super->super.super.super.super));
      return false;
    }


  this->schema_prototype = this->msg_factory->GetPrototype(this->schema_descriptor);
  this->protobuf_schema.loaded = true;
  return true;
}


/* C Wrappers */

DestinationDriver *
bigquery_dd_get_cpp(GrpcDestDriver *self)
{
  return (DestinationDriver *) self->cpp;
}

void
bigquery_dd_set_project(LogDriver *d, const gchar *project)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestinationDriver *cpp = bigquery_dd_get_cpp(self);
  cpp->set_project(project);
}

void
bigquery_dd_set_dataset(LogDriver *d, const gchar *dataset)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestinationDriver *cpp = bigquery_dd_get_cpp(self);
  cpp->set_dataset(dataset);
}

void bigquery_dd_set_table(LogDriver *d, const gchar *table)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestinationDriver *cpp = bigquery_dd_get_cpp(self);
  cpp->set_table(table);
}

gboolean
bigquery_dd_add_field(LogDriver *d, const gchar *name, const gchar *type, LogTemplate *value)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestinationDriver *cpp = bigquery_dd_get_cpp(self);
  return cpp->add_field(name, type ? type : "", value);
}

void
bigquery_dd_set_protobuf_schema(LogDriver *d, const gchar *proto_path, GList *values)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestinationDriver *cpp = bigquery_dd_get_cpp(self);
  cpp->set_protobuf_schema(proto_path, values);
}

LogDriver *
bigquery_dd_new(GlobalConfig *cfg)
{
  GrpcDestDriver *self = grpc_dd_new(cfg, "bigquery");
  self->cpp = new DestinationDriver(self);
  return &self->super.super.super;
}
