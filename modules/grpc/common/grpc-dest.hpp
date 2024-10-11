/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2023-2024 Attila Szakacs <attila.szakacs@axoflow.com>
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

#ifndef GRPC_DEST_HPP
#define GRPC_DEST_HPP

#include "grpc-dest.h"

#include "compat/cpp-start.h"
#include "logthrdest/logthrdestdrv.h"
#include "compat/cpp-end.h"

#include "credentials/grpc-credentials-builder.hpp"
#include "metrics/grpc-metrics.hpp"

#include <grpcpp/server.h>

#include <list>
#include <sstream>

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

class DestDriver
{
public:
  DestDriver(GrpcDestDriver *s);
  virtual ~DestDriver() {};

  virtual bool init();
  virtual bool deinit();
  virtual const char *format_stats_key(StatsClusterKeyBuilder *kb) = 0;
  virtual const char *generate_persist_name() = 0;
  virtual LogThreadedDestWorker *construct_worker(int worker_index) = 0;

  void set_url(const char *u)
  {
    this->url.assign(u);
  }

  const std::string &get_url() const
  {
    return this->url;
  }

  void set_compression(bool c)
  {
    this->compression = c;
  }

  bool get_compression() const
  {
    return this->compression;
  }

  void set_batch_bytes(size_t b)
  {
    this->batch_bytes = b;
  }

  size_t get_batch_bytes() const
  {
    return this->batch_bytes;
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
    this->int_extra_channel_args.push_back(std::make_pair(name, value));
  }

  void add_extra_channel_arg(std::string name, std::string value)
  {
    this->string_extra_channel_args.push_back(std::make_pair(name, value));
  }

  void add_header(std::string name, std::string value)
  {
    std::transform(name.begin(), name.end(), name.begin(),
                   [](auto c)
    {
      return ::tolower(c);
    });
    this->headers.push_back(std::make_pair(name, value));
  }

  void extend_worker_partition_key(const std::string &extension)
  {
    if (this->worker_partition_key.rdbuf()->in_avail())
      this->worker_partition_key << ",";
    this->worker_partition_key << extension;
  }

  GrpcClientCredentialsBuilderW *get_credentials_builder_wrapper()
  {
    return &this->credentials_builder_wrapper;
  }

private:
  bool set_worker_partition_key();

public:
  GrpcDestDriver *super;
  DestDriverMetrics metrics;
  syslogng::grpc::ClientCredentialsBuilder credentials_builder;

protected:
  friend class DestWorker;
  std::string url;

  bool compression;
  size_t batch_bytes;

  int keepalive_time;
  int keepalive_timeout;
  int keepalive_max_pings_without_data;

  std::stringstream worker_partition_key;
  bool flush_on_key_change;

  std::list<std::pair<std::string, long>> int_extra_channel_args;
  std::list<std::pair<std::string, std::string>> string_extra_channel_args;

  std::list<std::pair<std::string, std::string>> headers;

  GrpcClientCredentialsBuilderW credentials_builder_wrapper;
};

}
}

struct GrpcDestDriver_
{
  LogThreadedDestDriver super;
  syslogng::grpc::DestDriver *cpp;
};

GrpcDestDriver *grpc_dd_new(GlobalConfig *cfg, const gchar *stats_name);

#endif


