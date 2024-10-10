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

DestinationDriver::DestinationDriver(GrpcDestDriver *s)
  : syslogng::grpc::DestDriver(s), timestamp(LM_TS_PROCESSED)
{
  this->url = "localhost:9095";
  this->flush_on_key_change = true;
  log_template_options_defaults(&this->template_options);
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

  if (!this->message)
    {
      this->message = log_template_new(cfg, NULL);
      log_template_compile(this->message, DEFAULT_MESSAGE_TEMPLATE, NULL);
    }

  log_template_options_init(&this->template_options, cfg);

  for (const auto &label : this->labels)
    this->extend_worker_partition_key(label.name + "=" + label.value->template_str);

  return syslogng::grpc::DestDriver::init();
}

const gchar *
DestinationDriver::generate_persist_name()
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

LogThreadedDestWorker *
DestinationDriver::construct_worker(int worker_index)
{
  GrpcDestWorker *worker = grpc_dw_new(this->super, worker_index);
  worker->cpp = new DestinationWorker(worker);
  return &worker->super;
}

/* C Wrappers */

DestinationDriver *
loki_dd_get_cpp(GrpcDestDriver *self)
{
  return (DestinationDriver *) self->cpp;
}

void
loki_dd_set_message_template_ref(LogDriver *d, LogTemplate *message)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestinationDriver *cpp = loki_dd_get_cpp(self);
  cpp->set_message_template_ref(message);
}

void
loki_dd_add_label(LogDriver *d, const gchar *name, LogTemplate *value)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestinationDriver *cpp = loki_dd_get_cpp(self);
  cpp->add_label(name, value);
}

gboolean
loki_dd_set_timestamp(LogDriver *d, const gchar *t)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestinationDriver *cpp = loki_dd_get_cpp(self);
  return cpp->set_timestamp(t);
}

void
loki_dd_set_tenant_id(LogDriver *d, const gchar *tid)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestinationDriver *cpp = loki_dd_get_cpp(self);
  return cpp->set_tenant_id(tid);
}

LogTemplateOptions *
loki_dd_get_template_options(LogDriver *d)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestinationDriver *cpp = loki_dd_get_cpp(self);
  return &cpp->get_template_options();
}

LogDriver *
loki_dd_new(GlobalConfig *cfg)
{
  GrpcDestDriver *self = grpc_dd_new(cfg, "loki");
  self->cpp = new DestinationDriver(self);
  return &self->super.super.super;
}
