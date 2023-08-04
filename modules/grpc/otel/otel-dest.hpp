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

#include <grpcpp/server.h>

#include "compat/cpp-start.h"
#include "logthrdest/logthrdestdrv.h"
#include "compat/cpp-end.h"

#include "otel-dest.h"
#include "grpc-credentials-builder.hpp"


namespace syslogng {
namespace grpc {
namespace otel {

class DestDriver
{
public:
  DestDriver(OtelDestDriver *s);

  void set_url(const char *url);
  const std::string &get_url() const;

  bool init();
  bool deinit();
  const char *format_stats_key(StatsClusterKeyBuilder *kb);
  const char *generate_persist_name();

  GrpcClientCredentialsBuilderW *get_credentials_builder_wrapper();

public:
  syslogng::grpc::ClientCredentialsBuilder credentials_builder;

private:
  friend class DestWorker;
  OtelDestDriver *super;
  std::string url;
  GrpcClientCredentialsBuilderW credentials_builder_wrapper;
};

}
}
}

struct OtelDestDriver_
{
  LogThreadedDestDriver super;
  syslogng::grpc::otel::DestDriver *cpp;
};

#endif
