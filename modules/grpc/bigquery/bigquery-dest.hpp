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
#include "grpc-dest.hpp"

#include "compat/cpp-start.h"
#include "template/templates.h"
#include "stats/stats-cluster-key-builder.h"
#include "compat/cpp-end.h"

#include <string>

namespace syslogng {
namespace grpc {
namespace bigquery {

class DestinationDriver final : public syslogng::grpc::DestDriver
{
public:
  DestinationDriver(GrpcDestDriver *s);
  bool init();
  const gchar *generate_persist_name();
  const gchar *format_stats_key(StatsClusterKeyBuilder *kb);
  LogThreadedDestWorker *construct_worker(int worker_index);

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

  Schema *get_schema()
  {
    return &this->schema;
  }

private:
  static bool map_schema_type(const std::string &type_in, google::protobuf::FieldDescriptorProto::Type &type_out);

private:
  friend class DestinationWorker;

private:
  Schema schema;

  std::string project;
  std::string dataset;
  std::string table;
};


}
}
}

syslogng::grpc::bigquery::DestinationDriver *bigquery_dd_get_cpp(GrpcDestDriver *self);

#endif
