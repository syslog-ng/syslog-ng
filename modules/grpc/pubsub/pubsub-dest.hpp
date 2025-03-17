/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
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

#ifndef PUBSUB_DEST_HPP
#define PUBSUB_DEST_HPP

#include "pubsub-dest.h"
#include "grpc-dest.hpp"

#include <string>

namespace syslogng {
namespace grpc {
namespace pubsub {

class DestDriver final : public syslogng::grpc::DestDriver
{
public:
  DestDriver(GrpcDestDriver *s);
  bool init();
  const gchar *generate_persist_name();
  const gchar *format_stats_key(StatsClusterKeyBuilder *kb);
  LogThreadedDestWorker *construct_worker(int worker_index);

private:
  friend class DestWorker;
};


}
}
}

syslogng::grpc::pubsub::DestDriver *pubsub_dd_get_cpp(GrpcDestDriver *self);

#endif
