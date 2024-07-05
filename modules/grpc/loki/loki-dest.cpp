/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
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

#include "loki-dest.hpp"
#include "loki-worker.hpp"

#include "compat/cpp-start.h"
#include "logthrdest/logthrdestdrv.h"
#include "messages.h"
#include "template/templates.h"
#include "compat/cpp-end.h"

#include <cstring>
#include <string>
#include <sstream>

constexpr const auto DEFAULT_MESSAGE_TEMPLATE = "$ISODATE $HOST $MSGHDR$MSG";

using syslogng::grpc::loki::DestinationDriver;

struct _LokiDestDriver
{
  LogThreadedDestDriver super;
  DestinationDriver *cpp;
};

DestinationDriver::DestinationDriver(LokiDestDriver *s)
  : super(s), url("localhost:9095"), timestamp(LM_TS_PROCESSED),
    keepalive_time(-1), keepalive_timeout(-1), keepalive_max_pings_without_data(-1)
{
  log_template_options_defaults(&this->template_options);
  credentials_builder_wrapper.self = &credentials_builder;
}

DestinationDriver::~DestinationDriver()
{
  log_template_options_destroy(&this->template_options);
  log_template_unref(this->message);
}

void
DestinationDriver::add_label(std::string name, LogTemplate *value)
{
  this->labels.push_back(Label{name, value});
}

bool
DestinationDriver::init()
{
  GlobalConfig *cfg = log_pipe_get_config(&this->super->super.super.super.super);

  if (!credentials_builder.validate())
    {
      return false;
    }

  if (!this->message)
    {
      this->message = log_template_new(cfg, NULL);
      log_template_compile(this->message, DEFAULT_MESSAGE_TEMPLATE, NULL);
    }

  log_template_options_init(&this->template_options, cfg);

  LogTemplate *worker_partition_key = log_template_new(cfg, NULL);

  std::stringstream template_str;
  bool comma_needed = false;
  for (const auto &label : this->labels)
    {
      if (comma_needed)
        template_str << ",";
      template_str << label.name << "=" << label.value->template_str;

      comma_needed = true;
    }

  std::string worker_partition_key_str = template_str.str();
  if (!log_template_compile(worker_partition_key, worker_partition_key_str.c_str(), NULL))
    {
      msg_error("Error compiling worker partition key template",
                evt_tag_str("template", worker_partition_key_str.c_str()));
      return false;
    }

  if (log_template_is_literal_string(worker_partition_key))
    log_template_unref(worker_partition_key);
  else
    log_threaded_dest_driver_set_worker_partition_key_ref(&this->super->super.super.super, worker_partition_key);

  if (!log_threaded_dest_driver_init_method(&this->super->super.super.super.super))
    return false;

  StatsClusterKeyBuilder *kb = stats_cluster_key_builder_new();
  this->format_stats_key(kb);
  this->metrics.init(kb, log_pipe_is_internal(&this->super->super.super.super.super) ? STATS_LEVEL3 : STATS_LEVEL1);

  return true;
}

bool
DestinationDriver::deinit()
{
  this->metrics.deinit();
  return log_threaded_dest_driver_deinit_method(&this->super->super.super.super.super);
}

const gchar *
DestinationDriver::format_persist_name()
{
  static gchar persist_name[1024];

  LogPipe *s = &this->super->super.super.super.super;
  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "loki.%s", s->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "loki(%s)", this->url.c_str());

  return persist_name;
}

const gchar *
DestinationDriver::format_stats_key(StatsClusterKeyBuilder *kb)
{
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "loki"));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("url", this->url.c_str()));

  return nullptr;
}

GrpcClientCredentialsBuilderW *
DestinationDriver::get_credentials_builder_wrapper()
{
  return &this->credentials_builder_wrapper;
}


/* C Wrappers */

DestinationDriver *
loki_dd_get_cpp(LokiDestDriver *self)
{
  return self->cpp;
}

static const gchar *
_format_persist_name(const LogPipe *s)
{
  LokiDestDriver *self = (LokiDestDriver *) s;
  return self->cpp->format_persist_name();
}

static const gchar *
_format_stats_key(LogThreadedDestDriver *s, StatsClusterKeyBuilder *kb)
{
  LokiDestDriver *self = (LokiDestDriver *) s;
  return self->cpp->format_stats_key(kb);
}

GrpcClientCredentialsBuilderW *
loki_dd_get_credentials_builder(LogDriver *s)
{
  LokiDestDriver *self = (LokiDestDriver *) s;
  return self->cpp->get_credentials_builder_wrapper();
}

void
loki_dd_set_url(LogDriver *d, const gchar *url)
{
  LokiDestDriver *self = (LokiDestDriver *) d;
  self->cpp->set_url(url);
}

void
loki_dd_set_message_template_ref(LogDriver *d, LogTemplate *message)
{
  LokiDestDriver *self = (LokiDestDriver *) d;
  self->cpp->set_message_template_ref(message);
}

void
loki_dd_add_label(LogDriver *d, const gchar *name, LogTemplate *value)
{
  LokiDestDriver *self = (LokiDestDriver *) d;
  self->cpp->add_label(name, value);
}

gboolean
loki_dd_set_timestamp(LogDriver *d, const gchar *t)
{
  LokiDestDriver *self = (LokiDestDriver *) d;
  return self->cpp->set_timestamp(t);
}

void
loki_dd_set_tenant_id(LogDriver *d, const gchar *tid)
{
  LokiDestDriver *self = (LokiDestDriver *) d;
  return self->cpp->set_tenant_id(tid);
}

void
loki_dd_set_keepalive_time(LogDriver *d, gint t)
{
  LokiDestDriver *self = (LokiDestDriver *) d;
  self->cpp->set_keepalive_time(t);
}

void
loki_dd_set_keepalive_timeout(LogDriver *d, gint t)
{
  LokiDestDriver *self = (LokiDestDriver *) d;
  self->cpp->set_keepalive_timeout(t);
}

void
loki_dd_set_keepalive_max_pings(LogDriver *d, gint p)
{
  LokiDestDriver *self = (LokiDestDriver *) d;
  self->cpp->set_keepalive_max_pings(p);
}

void
loki_dd_add_int_channel_arg(LogDriver *d, const gchar *name, glong value)
{
  LokiDestDriver *self = (LokiDestDriver *) d;
  self->cpp->add_extra_channel_arg(name, value);
}

void
loki_dd_add_string_channel_arg(LogDriver *d, const gchar *name, const gchar *value)
{
  LokiDestDriver *self = (LokiDestDriver *) d;
  self->cpp->add_extra_channel_arg(name, value);
}

void
loki_dd_add_header(LogDriver *d, const gchar *name, const gchar *value)
{
  LokiDestDriver *self = (LokiDestDriver *) d;
  self->cpp->add_header(name, value);
}

LogTemplateOptions *
loki_dd_get_template_options(LogDriver *d)
{
  LokiDestDriver *self = (LokiDestDriver *) d;
  return &self->cpp->get_template_options();
}

static gboolean
_init(LogPipe *s)
{
  LokiDestDriver *self = (LokiDestDriver *) s;
  return self->cpp->init();
}

static gboolean
_deinit(LogPipe *s)
{
  LokiDestDriver *self = (LokiDestDriver *) s;
  return self->cpp->deinit();
}

static void
_free(LogPipe *s)
{
  LokiDestDriver *self = (LokiDestDriver *) s;
  delete self->cpp;

  log_threaded_dest_driver_free(s);
}

LogDriver *
loki_dd_new(GlobalConfig *cfg)
{
  LokiDestDriver *self = g_new0(LokiDestDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  self->cpp = new DestinationDriver(self);

  self->super.super.super.super.init = _init;
  self->super.super.super.super.deinit = _deinit;
  self->super.super.super.super.free_fn = _free;
  self->super.super.super.super.generate_persist_name = _format_persist_name;

  self->super.format_stats_key = _format_stats_key;
  self->super.stats_source = stats_register_type("loki");

  self->super.worker.construct = loki_dw_new;

  self->super.flush_on_key_change = TRUE;

  return &self->super.super.super;
}
