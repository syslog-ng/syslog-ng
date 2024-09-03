/*
 * Copyright (c) 2024 Attila Szakacs
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

#ifndef GRPC_METRICS_HPP
#define GRPC_METRICS_HPP

#include "syslog-ng.h"

#include "compat/cpp-start.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster-single.h"
#include "stats/stats-cluster-key-builder.h"
#include "compat/cpp-end.h"

#include <grpc++/grpc++.h>
#include <map>

namespace syslogng {
namespace grpc {

class DestDriverMetrics
{
public:
  void init(StatsClusterKeyBuilder *kb, int stats_level);
  void deinit();

  void insert_grpc_request_stats(const ::grpc::Status &response_status);

private:
  StatsCluster *create_grpc_request_cluster(::grpc::StatusCode response_code);
  StatsCounterItem *lookup_grpc_request_counter(::grpc::StatusCode response_code);

private:
  StatsClusterKeyBuilder *kb;
  int stats_level;

  std::map<::grpc::StatusCode, StatsCluster *> grpc_request_clusters;
};

}
}

#endif
