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

#include <cstring>
#include <strings.h>

using syslogng::grpc::bigquery::DestinationDriver;

DestinationDriver::DestinationDriver(GrpcDestDriver *s)
  : syslogng::grpc::DestDriver(s),
    schema(2, "bigquery_record.proto", "BigQueryRecord", map_schema_type,
           &this->template_options, &this->super->super.super.super.super)
{
  this->url = "bigquerystorage.googleapis.com";
  this->credentials_builder.set_mode(GCAM_ADC);
}

bool
DestinationDriver::map_schema_type(const std::string &type_in, google::protobuf::FieldDescriptorProto::Type &type_out)
{
  /* https://cloud.google.com/bigquery/docs/write-api#data_type_conversions */

  const char *type_str = type_in.c_str();
  if (type_in.empty() || strcasecmp(type_str, "STRING") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else if (strcasecmp(type_str, "BYTES") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_BYTES;
  else if (strcasecmp(type_str, "INTEGER") == 0 || strcasecmp(type_str, "INT64") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_INT64;
  else if (strcasecmp(type_str, "FLOAT") == 0 || strcasecmp(type_str, "FLOAT64") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_DOUBLE;
  else if (strcasecmp(type_str, "BOOLEAN") == 0 || strcasecmp(type_str, "BOOL") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_BOOL;
  else if (strcasecmp(type_str, "TIMESTAMP") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_INT64;
  else if (strcasecmp(type_str, "DATE") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_INT32;
  else if (strcasecmp(type_str, "TIME") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else if (strcasecmp(type_str, "DATETIME") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else if (strcasecmp(type_str, "JSON") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else if (strcasecmp(type_str, "NUMERIC") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_INT64;
  else if (strcasecmp(type_str, "BIGNUMERIC") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else if (strcasecmp(type_str, "GEOGRAPHY") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else if (strcasecmp(type_str, "RECORD") == 0 || strcasecmp(type_str, "STRUCT") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_MESSAGE;
  else if (strcasecmp(type_str, "INTERVAL") == 0)
    type_out = google::protobuf::FieldDescriptorProto::TYPE_STRING;
  else
    return false;
  return true;
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

  if (!this->schema.init())
    return false;

  if (this->schema.empty())
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

LogDriver *
bigquery_dd_new(GlobalConfig *cfg)
{
  GrpcDestDriver *self = grpc_dd_new(cfg, "bigquery");
  self->cpp = new DestinationDriver(self);
  return &self->super.super.super;
}
