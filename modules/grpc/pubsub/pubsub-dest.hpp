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
#include <vector>

namespace syslogng {
namespace grpc {
namespace pubsub {

class DestDriver final : public syslogng::grpc::DestDriver
{
public:
  DestDriver(GrpcDestDriver *s);
  ~DestDriver();
  bool init();
  const gchar *generate_persist_name();
  const gchar *format_stats_key(StatsClusterKeyBuilder *kb);
  LogThreadedDestWorker *construct_worker(int worker_index);

  void set_project(LogTemplate *p)
  {
    log_template_unref(this->project);
    this->project = log_template_ref(p);
  }

  void set_topic(LogTemplate *t)
  {
    log_template_unref(this->topic);
    this->topic = log_template_ref(t);
  }

  void set_data(LogTemplate *d)
  {
    log_template_unref(this->data);
    this->data = log_template_ref(d);
  }

  void set_protovar(LogTemplate *p)
  {
    log_template_unref(this->protovar);
    this->protovar = log_template_ref(p);
  }

  void add_attribute(const std::string &name, LogTemplate *value)
  {
    this->attributes.push_back(NameValueTemplatePair{name, value});
  }

private:
  friend class DestWorker;

private:
  LogTemplate *project = nullptr;
  LogTemplate *topic = nullptr;
  LogTemplate *data = nullptr;
  LogTemplate *protovar = nullptr;
  LogTemplate *default_data_template = nullptr;
  std::vector<NameValueTemplatePair> attributes;
};


}
}
}

syslogng::grpc::pubsub::DestDriver *pubsub_dd_get_cpp(GrpcDestDriver *self);

#endif
