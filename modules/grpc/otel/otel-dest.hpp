/*
 * Copyright (c) 2023 Attila Szakacs
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

#ifndef OTEL_DEST_HPP
#define OTEL_DEST_HPP

#include "otel-dest.h"

#include "compat/cpp-start.h"
#include "logthrdest/logthrdestdrv.h"
#include "compat/cpp-end.h"

#include "credentials/grpc-credentials-builder.hpp"
#include "metrics/grpc-metrics.hpp"

#include <grpcpp/server.h>

#include <list>

namespace syslogng {
namespace grpc {
namespace otel {

class DestDriver
{
public:
  DestDriver(OtelDestDriver *s);
  virtual ~DestDriver() {};

  void set_url(const char *url);
  const std::string &get_url() const;

  void set_compression(bool enable);
  bool get_compression() const;

  void set_batch_bytes(size_t bytes);
  size_t get_batch_bytes() const;

  void add_extra_channel_arg(std::string name, long value);
  void add_extra_channel_arg(std::string name, std::string value);

  virtual bool init();
  virtual bool deinit();
  virtual const char *format_stats_key(StatsClusterKeyBuilder *kb);
  virtual const char *generate_persist_name();
  virtual LogThreadedDestWorker *construct_worker(int worker_index);

  GrpcClientCredentialsBuilderW *get_credentials_builder_wrapper();

public:
  syslogng::grpc::ClientCredentialsBuilder credentials_builder;

protected:
  friend class DestWorker;
  OtelDestDriver *super;
  std::string url;
  bool compression;
  size_t batch_bytes;
  std::list<std::pair<std::string, long>> int_extra_channel_args;
  std::list<std::pair<std::string, std::string>> string_extra_channel_args;
  GrpcClientCredentialsBuilderW credentials_builder_wrapper;
  DestDriverMetrics metrics;
};

}
}
}

struct OtelDestDriver_
{
  LogThreadedDestDriver super;
  syslogng::grpc::otel::DestDriver *cpp;
};

void otel_dd_init_super(LogThreadedDestDriver *s, GlobalConfig *cfg);

#endif
