/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2023-2024 Attila Szakacs <attila.szakacs@axoflow.com>
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

#ifndef GRPC_SOURCE_HPP
#define GRPC_SOURCE_HPP

#include "grpc-source.h"

#include "compat/cpp-start.h"
#include "logthrsource/logthrsourcedrv.h"
#include "compat/cpp-end.h"

#include "credentials/grpc-credentials-builder.hpp"

#include <grpcpp/grpcpp.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <list>

namespace syslogng {
namespace grpc {

class SourceDriver
{
public:
  SourceDriver(GrpcSourceDriver *s);
  virtual ~SourceDriver() {};

  virtual gboolean init();
  virtual gboolean deinit();
  virtual void request_exit() = 0;
  virtual void format_stats_key(StatsClusterKeyBuilder *kb) = 0;
  virtual const char *generate_persist_name() = 0;
  virtual LogThreadedSourceWorker *construct_worker(int worker_index) = 0;

  void set_port(unsigned int p)
  {
    this->port = p;
  }

  unsigned int get_port()
  {
    return this->port;
  }

  void set_fetch_limit(int f)
  {
    this->fetch_limit = f;
  }

  int get_fetch_limit()
  {
    return this->fetch_limit;
  }

  void set_concurrent_requests(int c)
  {
    this->concurrent_requests = c;
  }

  int get_concurrent_requests()
  {
    return this->concurrent_requests;
  }

  void add_extra_channel_arg(std::string name, long value)
  {
    this->int_extra_channel_args.push_back(std::pair<std::string, long> {name, value});
  }

  void add_extra_channel_arg(std::string name, std::string value)
  {
    this->string_extra_channel_args.push_back(std::pair<std::string, std::string> {name, value});
  }

  GrpcServerCredentialsBuilderW *get_credentials_builder_wrapper()
  {
    return &this->credentials_builder_wrapper;
  }

protected:
  bool prepare_server_builder(::grpc::ServerBuilder &builder);

private:
  friend class SourceWorker;

public:
  GrpcSourceDriver *super;
  syslogng::grpc::ServerCredentialsBuilder credentials_builder;

protected:
  unsigned int port = 0;
  int fetch_limit = -1;
  int concurrent_requests = 2;
  std::list<std::pair<std::string, long>> int_extra_channel_args;
  std::list<std::pair<std::string, std::string>> string_extra_channel_args;

private:
  GrpcServerCredentialsBuilderW credentials_builder_wrapper;
  std::unique_ptr<::grpc::Server> server;
};

}
}

struct GrpcSourceDriver_
{
  LogThreadedSourceDriver super;
  syslogng::grpc::SourceDriver *cpp;
};

GrpcSourceDriver *grpc_sd_new(GlobalConfig *cfg, const gchar *stats_name, const gchar *transport_name);

#endif
