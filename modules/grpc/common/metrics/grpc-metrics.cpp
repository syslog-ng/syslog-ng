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

#include "grpc-metrics.hpp"

#include "compat/cpp-start.h"
#include "messages.h"
#include "compat/cpp-end.h"

using namespace syslogng::grpc;

/*
 * Initializes the DestDriverMetrics instance.
 * Takes the ownership of kb.
 */
void
DestDriverMetrics::init(StatsClusterKeyBuilder *kb_, int stats_level_)
{
  kb = kb_;
  stats_level = stats_level_;
}

void
DestDriverMetrics::deinit()
{
  stats_lock();
  {
    for (const auto &clusters : grpc_request_clusters)
      {
        StatsCounterItem *counter = stats_cluster_single_get_counter(clusters.second);
        stats_unregister_counter(&clusters.second->key, SC_TYPE_SINGLE_VALUE, &counter);
      }
  }
  stats_unlock();

  stats_cluster_key_builder_free(kb);
}

StatsCluster *
DestDriverMetrics::create_grpc_request_cluster(::grpc::StatusCode response_code)
{
  static const std::map<::grpc::StatusCode, std::string> status_code_name_mappings =
  {
    {::grpc::StatusCode::OK, "ok"},
    {::grpc::StatusCode::CANCELLED, "cancelled"},
    {::grpc::StatusCode::UNKNOWN, "unknown"},
    {::grpc::StatusCode::INVALID_ARGUMENT, "invalid_argument"},
    {::grpc::StatusCode::DEADLINE_EXCEEDED, "deadline_exceeded"},
    {::grpc::StatusCode::NOT_FOUND, "not_found"},
    {::grpc::StatusCode::ALREADY_EXISTS, "already_exists"},
    {::grpc::StatusCode::PERMISSION_DENIED, "permission_denied"},
    {::grpc::StatusCode::UNAUTHENTICATED, "unauthenticated"},
    {::grpc::StatusCode::RESOURCE_EXHAUSTED, "resource_exhausted"},
    {::grpc::StatusCode::FAILED_PRECONDITION, "failed_precondition"},
    {::grpc::StatusCode::ABORTED, "aborted"},
    {::grpc::StatusCode::OUT_OF_RANGE, "out_of_range"},
    {::grpc::StatusCode::UNIMPLEMENTED, "unimplemented"},
    {::grpc::StatusCode::INTERNAL, "internal"},
    {::grpc::StatusCode::UNAVAILABLE, "unavailable"},
    {::grpc::StatusCode::DATA_LOSS, "data_loss"},
  };

  std::string response_code_label;
  try
    {
      response_code_label = status_code_name_mappings.at(response_code);
    }
  catch (const std::out_of_range &)
    {
      msg_error("Failed to find metric label for gRPC response code", evt_tag_int("response_code", response_code));
      return nullptr;
    }

  StatsCluster *cluster;
  stats_cluster_key_builder_push(kb);
  {
    stats_cluster_key_builder_set_name(kb, "output_grpc_requests_total");
    stats_cluster_key_builder_add_label(kb, stats_cluster_label("response_code", response_code_label.c_str()));
    StatsClusterKey *sc_key = stats_cluster_key_builder_build_single(kb);

    StatsCounterItem *counter;
    cluster = stats_register_counter(stats_level, sc_key, SC_TYPE_SINGLE_VALUE, &counter);

    stats_cluster_key_free(sc_key);
  }
  stats_cluster_key_builder_pop(kb);

  return cluster;
}

StatsCounterItem *
DestDriverMetrics::lookup_grpc_request_counter(::grpc::StatusCode response_code)
{
  StatsCluster *cluster;

  try
    {
      cluster = grpc_request_clusters.at(response_code);
    }
  catch (const std::out_of_range &)
    {
      stats_lock();
      {
        try
          {
            cluster = grpc_request_clusters.at(response_code);
          }
        catch (const std::out_of_range &)
          {
            grpc_request_clusters[response_code] = cluster = create_grpc_request_cluster(response_code);
          }
      }
      stats_unlock();
    }

  return stats_cluster_single_get_counter(cluster);
}

void
DestDriverMetrics::insert_grpc_request_stats(const ::grpc::Status &response_status)
{
  StatsCounterItem *counter = lookup_grpc_request_counter(response_status.error_code());
  stats_counter_inc(counter);
}
