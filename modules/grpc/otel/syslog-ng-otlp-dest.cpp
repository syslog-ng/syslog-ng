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

#include "syslog-ng-otlp-dest.hpp"
#include "syslog-ng-otlp-dest-worker.hpp"

using namespace syslogng::grpc::otel;

/* C++ Implementations */

const char *
SyslogNgDestDriver::generate_legacy_persist_name()
{
  static char persist_name[1024];

  if (super->super.super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "syslog-ng-otlp.%s",
               super->super.super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "syslog-ng-otlp");

  return persist_name;
}

const char *
SyslogNgDestDriver::generate_persist_name()
{
  static char persist_name[1024];

  if (super->super.super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "syslog-ng-otlp.%s",
               super->super.super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "syslog-ng-otlp(%s)",
               url.c_str());

  return persist_name;
}

const char *
SyslogNgDestDriver::format_stats_key(StatsClusterKeyBuilder *kb)
{
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "syslog-ng-otlp"));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("url", url.c_str()));

  return NULL;
}

LogThreadedDestWorker *
SyslogNgDestDriver::construct_worker(int worker_index)
{
  GrpcDestWorker *worker = grpc_dw_new(this->super, worker_index);
  worker->cpp = new SyslogNgDestWorker(worker);
  return &worker->super;
}

bool
SyslogNgDestDriver::update_legacy_persist_name_if_exists()
{
  GlobalConfig *cfg = log_pipe_get_config(&super->super.super.super.super);
  const char *current_persist_name = generate_persist_name();
  const char *legacy_persist_name = generate_legacy_persist_name();

  if (persist_state_entry_exists(cfg->state, current_persist_name))
    return true;

  if (!persist_state_entry_exists(cfg->state, legacy_persist_name))
    return true;

  if (strcmp(current_persist_name, legacy_persist_name) == 0)
    return true;

  return persist_state_move_entry(cfg->state, legacy_persist_name, current_persist_name);
}

bool
SyslogNgDestDriver::init()
{
  if (!DestDriver::init())
    return false;

  if (!update_legacy_persist_name_if_exists())
    return false;

  return true;
}

/* C Wrappers */

LogDriver *
syslog_ng_otlp_dd_new(GlobalConfig *cfg)
{
  GrpcDestDriver *self = grpc_dd_new(cfg, "syslog-ng-otlp");
  self->cpp = new SyslogNgDestDriver(self);
  return &self->super.super.super;
}
