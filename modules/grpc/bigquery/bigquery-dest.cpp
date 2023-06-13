/*
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

#include "compat/cpp-start.h"
#include "logthrdest/logthrdestdrv.h"
#include "messages.h"
#include "compat/cpp-end.h"

#include <cstring>

using syslog_ng::bigquery::DestinationDriver;

struct _BigQueryDestDriver
{
  LogThreadedDestDriver super;
  DestinationDriver *cpp;
};


DestinationDriver::DestinationDriver(BigQueryDestDriver *s)
  : super(s), url("bigquerystorage.googleapis.com"), msg_factory(&descriptor_pool)
{
  log_template_options_defaults(&this->template_options);
}

DestinationDriver::~DestinationDriver()
{
  g_list_free_full(this->protobuf_schema.values, g_free);
  log_template_options_destroy(&this->template_options);
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

  g_list_free_full(this->protobuf_schema.values, g_free);
  this->protobuf_schema.values = g_list_copy_deep(values, ((GCopyFunc)g_strdup), NULL);
}

bool
DestinationDriver::init()
{
  GlobalConfig *cfg = log_pipe_get_config(&this->super->super.super.super.super);
  log_template_options_init(&this->template_options, cfg);

  this->construct_schema_prototype();

  return log_threaded_dest_driver_init_method(&this->super->super.super.super.super);
}

bool
DestinationDriver::deinit()
{
  return log_threaded_dest_driver_deinit_method(&this->super->super.super.super.super);
}

const gchar *
DestinationDriver::format_persist_name()
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

void
DestinationDriver::construct_schema_prototype()
{
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
      field_desc_proto->set_name(field.name);
      field_desc_proto->set_type(field.type);
      field_desc_proto->set_number(num++);
    }


  const google::protobuf::FileDescriptor *file_descriptor = this->descriptor_pool.BuildFile(file_descriptor_proto);
  this->schema_descriptor = file_descriptor->message_type(0);

  for (int i = 0; i < this->schema_descriptor->field_count(); ++i)
    {
      this->fields[i].field_desc = this->schema_descriptor->field(i);
    }

  this->schema_prototype = msg_factory.GetPrototype(this->schema_descriptor);
}


/* C Wrappers */

DestinationDriver *
bigquery_dd_get_cpp(BigQueryDestDriver *self)
{
  return self->cpp;
}

static const gchar *
_format_persist_name(const LogPipe *s)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) s;
  return self->cpp->format_persist_name();
}

static const gchar *
_format_stats_key(LogThreadedDestDriver *s, StatsClusterKeyBuilder *kb)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) s;
  return self->cpp->format_stats_key(kb);
}

void
bigquery_dd_set_url(LogDriver *d, const gchar *url)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) d;
  self->cpp->set_url(url);
}

void
bigquery_dd_set_project(LogDriver *d, const gchar *project)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) d;
  self->cpp->set_project(project);
}

void
bigquery_dd_set_dataset(LogDriver *d, const gchar *dataset)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) d;
  self->cpp->set_dataset(dataset);
}

void bigquery_dd_set_table(LogDriver *d, const gchar *table)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) d;
  self->cpp->set_table(table);
}

gboolean
bigquery_dd_add_field(LogDriver *d, const gchar *name, const gchar *type, LogTemplate *value)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) d;
  return self->cpp->add_field(name, type ? type : "", value);
}

void
bigquery_dd_set_protobuf_schema(LogDriver *d, const gchar *proto_path, GList *values)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) d;
  self->cpp->set_protobuf_schema(proto_path, values);
}

LogTemplateOptions *
bigquery_dd_get_template_options(LogDriver *d)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) d;
  return &self->cpp->get_template_options();
}

static gboolean
_init(LogPipe *s)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) s;
  return self->cpp->init();
}

static gboolean
_deinit(LogPipe *s)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) s;
  return self->cpp->deinit();
}

static void
_free(LogPipe *s)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) s;
  delete self->cpp;

  log_threaded_dest_driver_free(s);
}

LogDriver *
bigquery_dd_new(GlobalConfig *cfg)
{
  BigQueryDestDriver *self = g_new0(BigQueryDestDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  self->cpp = new DestinationDriver(self);

  self->super.super.super.super.init = _init;
  self->super.super.super.super.deinit = _deinit;
  self->super.super.super.super.free_fn = _free;
  self->super.super.super.super.generate_persist_name = _format_persist_name;

  self->super.format_stats_key = _format_stats_key;
  self->super.stats_source = stats_register_type("bigquery");

  self->super.worker.construct = bigquery_dw_new;

  return &self->super.super.super;
}
