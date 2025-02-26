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

#include "grpc-dest.hpp"
#include "grpc-dest-worker.hpp"

using namespace syslogng::grpc;

/* C++ Implementations */

DestDriver::DestDriver(GrpcDestDriver *s)
  : super(s), compression(false), batch_bytes(4 * 1000 * 1000),
    keepalive_time(-1), keepalive_timeout(-1), keepalive_max_pings_without_data(-1),
    flush_on_key_change(false), dynamic_headers_enabled(false)
{
  log_template_options_defaults(&this->template_options);
  credentials_builder_wrapper.self = &credentials_builder;
}

DestDriver::~DestDriver()
{
  log_template_options_destroy(&this->template_options);
}

bool
DestDriver::set_worker_partition_key()
{
  GlobalConfig *cfg = log_pipe_get_config(&this->super->super.super.super.super);

  LogTemplate *worker_partition_key_tpl = log_template_new(cfg, NULL);
  if (!log_template_compile(worker_partition_key_tpl, this->worker_partition_key.str().c_str(), NULL))
    {
      msg_error("Error compiling worker partition key template",
                evt_tag_str("template", this->worker_partition_key.str().c_str()));
      return false;
    }

  if (log_template_is_literal_string(worker_partition_key_tpl))
    {
      log_template_unref(worker_partition_key_tpl);
    }
  else
    {
      log_threaded_dest_driver_set_worker_partition_key_ref(&this->super->super.super.super, worker_partition_key_tpl);
      log_threaded_dest_driver_set_flush_on_worker_key_change(&this->super->super.super.super,
                                                              this->flush_on_key_change);
    }

  return true;
}

bool
DestDriver::init()
{
  GlobalConfig *cfg = log_pipe_get_config(&this->super->super.super.super.super);

  if (url.length() == 0)
    {
      msg_error("url() option is mandatory",
                log_pipe_location_tag(&super->super.super.super.super));
      return false;
    }

  if (!credentials_builder.validate())
    {
      return false;
    }

  if (this->worker_partition_key.rdbuf()->in_avail() && !this->set_worker_partition_key())
    return false;

  log_template_options_init(&this->template_options, cfg);

  if (!log_threaded_dest_driver_init_method(&this->super->super.super.super.super))
    return false;

  log_threaded_dest_driver_register_aggregated_stats(&this->super->super);

  StatsClusterKeyBuilder *kb = stats_cluster_key_builder_new();
  format_stats_key(kb);
  metrics.init(kb, log_pipe_is_internal(&super->super.super.super.super) ? STATS_LEVEL3 : STATS_LEVEL1);

  return true;
}

bool
DestDriver::deinit()
{
  metrics.deinit();
  return log_threaded_dest_driver_deinit_method(&super->super.super.super.super);
}

/* C Wrappers */

void
grpc_dd_set_url(LogDriver *s, const gchar *url)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  self->cpp->set_url(url);
}

void
grpc_dd_set_compression(LogDriver *s, gboolean enable)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  self->cpp->set_compression(enable);
}

void
grpc_dd_set_batch_bytes(LogDriver *s, glong b)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  self->cpp->set_batch_bytes((size_t) b);
}

void
grpc_dd_set_keepalive_time(LogDriver *s, gint t)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  self->cpp->set_keepalive_time(t);
}

void
grpc_dd_set_keepalive_timeout(LogDriver *s, gint t)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  self->cpp->set_keepalive_timeout(t);
}

void
grpc_dd_set_keepalive_max_pings(LogDriver *s, gint p)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  self->cpp->set_keepalive_max_pings(p);
}

void
grpc_dd_add_int_channel_arg(LogDriver *s, const gchar *name, glong value)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  self->cpp->add_extra_channel_arg(name, value);
}

void
grpc_dd_add_string_channel_arg(LogDriver *s, const gchar *name, const gchar *value)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  self->cpp->add_extra_channel_arg(name, value);
}

gboolean
grpc_dd_add_header(LogDriver *s, const gchar *name, LogTemplate *value)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  return self->cpp->add_header(name, value);
}

LogTemplateOptions *
grpc_dd_get_template_options(LogDriver *d)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  return &self->cpp->get_template_options();
}

GrpcClientCredentialsBuilderW *
grpc_dd_get_credentials_builder(LogDriver *s)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  return self->cpp->get_credentials_builder_wrapper();
}

static const gchar *
_format_stats_key(LogThreadedDestDriver *s, StatsClusterKeyBuilder *kb)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  return self->cpp->format_stats_key(kb);
}

static const gchar *
_generate_persist_name(const LogPipe *s)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  return self->cpp->generate_persist_name();
}

static gboolean
_init(LogPipe *s)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  return self->cpp->init();
}

static gboolean
_deinit(LogPipe *s)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  return self->cpp->deinit();
}

static LogThreadedDestWorker *
_construct_worker(LogThreadedDestDriver *s, gint worker_index)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  return self->cpp->construct_worker(worker_index);
}

static void
_free(LogPipe *s)
{
  GrpcDestDriver *self = (GrpcDestDriver *) s;
  delete self->cpp;
  log_threaded_dest_driver_free(s);
}

GrpcDestDriver *
grpc_dd_new(GlobalConfig *cfg, const gchar *stats_name)
{
  GrpcDestDriver *self = g_new0(GrpcDestDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  self->super.super.super.super.init = _init;
  self->super.super.super.super.deinit = _deinit;
  self->super.super.super.super.free_fn = _free;
  self->super.super.super.super.generate_persist_name = _generate_persist_name;

  self->super.worker.construct = _construct_worker;
  self->super.stats_source = stats_register_type(stats_name);
  self->super.format_stats_key = _format_stats_key;
  self->super.metrics.raw_bytes_enabled = TRUE;

  return self;
}
