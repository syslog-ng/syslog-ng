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

#include "grpc-source.hpp"

#include <string>

using namespace syslogng::grpc;

/* C++ Implementations */

SourceDriver::SourceDriver(GrpcSourceDriver *s)
  : super(s)
{
  credentials_builder_wrapper.self = &credentials_builder;
}

gboolean
SourceDriver::init()
{
  if (!this->port)
    {
      msg_error("Failed to initialize gRPC based source, port() must be set",
                log_pipe_location_tag(&this->super->super.super.super.super));
      return FALSE;
    }

  ::grpc::EnableDefaultHealthCheckService(true);

  if (this->fetch_limit == -1)
    {
      if (this->super->super.worker_options.super.init_window_size != -1)
        this->fetch_limit = this->super->super.worker_options.super.init_window_size / this->super->super.num_workers;
      else
        this->fetch_limit = 100;
    }

  return log_threaded_source_driver_init_method(&this->super->super.super.super.super);
}

gboolean
SourceDriver::deinit()
{
  return log_threaded_source_driver_deinit_method(&super->super.super.super.super);
}

bool
SourceDriver::prepare_server_builder(::grpc::ServerBuilder &builder)
{
  if (!this->credentials_builder.validate())
    return false;

  std::string address = std::string("[::]:").append(std::to_string(port));

  builder.AddListeningPort(address, credentials_builder.build());

  for (auto nv : int_extra_channel_args)
    builder.AddChannelArgument(nv.first, nv.second);
  for (auto nv : string_extra_channel_args)
    builder.AddChannelArgument(nv.first, nv.second);

  return true;
}

/* C Wrappers */

void
grpc_sd_set_port(LogDriver *s, guint64 port)
{
  GrpcSourceDriver *self = (GrpcSourceDriver *) s;
  self->cpp->set_port(port);
}

void
grpc_sd_set_fetch_limit(LogDriver *s, gint fetch_limit)
{
  GrpcSourceDriver *self = (GrpcSourceDriver *) s;
  self->cpp->set_fetch_limit(fetch_limit);
}

void
grpc_sd_set_concurrent_requests(LogDriver *s, gint concurrent_requests)
{
  GrpcSourceDriver *self = (GrpcSourceDriver *) s;
  self->cpp->set_concurrent_requests(concurrent_requests);
}

void
grpc_sd_add_int_channel_arg(LogDriver *s, const gchar *name, gint64 value)
{
  GrpcSourceDriver *self = (GrpcSourceDriver *) s;
  self->cpp->add_extra_channel_arg(name, value);
}

void
grpc_sd_add_string_channel_arg(LogDriver *s, const gchar *name, const gchar *value)
{
  GrpcSourceDriver *self = (GrpcSourceDriver *) s;
  self->cpp->add_extra_channel_arg(name, value);
}

GrpcServerCredentialsBuilderW *
grpc_sd_get_credentials_builder(LogDriver *s)
{
  GrpcSourceDriver *self = (GrpcSourceDriver *) s;
  return self->cpp->get_credentials_builder_wrapper();
}

static LogThreadedSourceWorker *
_construct_worker(LogThreadedSourceDriver *s, gint worker_index)
{
  GrpcSourceDriver *self = (GrpcSourceDriver *) s;
  return self->cpp->construct_worker(worker_index);
}

static void
_format_stats_key(LogThreadedSourceDriver *s, StatsClusterKeyBuilder *kb)
{
  GrpcSourceDriver *self = (GrpcSourceDriver *) s;
  self->cpp->format_stats_key(kb);
}

static const gchar *
_generate_persist_name(const LogPipe *s)
{
  GrpcSourceDriver *self = (GrpcSourceDriver *) s;
  return self->cpp->generate_persist_name();
}

static gboolean
_init(LogPipe *s)
{
  GrpcSourceDriver *self = (GrpcSourceDriver *) s;
  return self->cpp->init();
}

static gboolean
_deinit(LogPipe *s)
{
  GrpcSourceDriver *self = (GrpcSourceDriver *) s;
  return self->cpp->deinit();
}

static void
_free(LogPipe *s)
{
  GrpcSourceDriver *self = (GrpcSourceDriver *) s;
  delete self->cpp;
  log_threaded_source_driver_free_method(s);
}

GrpcSourceDriver *
grpc_sd_new(GlobalConfig *cfg, const gchar *stats_name, const gchar *transport_name)
{
  GrpcSourceDriver *self = g_new0(GrpcSourceDriver, 1);
  log_threaded_source_driver_init_instance(&self->super, cfg);
  log_threaded_source_driver_set_transport_name(&self->super, transport_name);

  self->super.super.super.super.init = _init;
  self->super.super.super.super.deinit = _deinit;
  self->super.super.super.super.free_fn = _free;
  self->super.super.super.super.generate_persist_name = _generate_persist_name;

  self->super.worker_options.super.stats_source = stats_register_type(stats_name);
  self->super.format_stats_key = _format_stats_key;
  self->super.worker_construct = _construct_worker;

  self->super.auto_close_batches = FALSE;

  return self;
}
