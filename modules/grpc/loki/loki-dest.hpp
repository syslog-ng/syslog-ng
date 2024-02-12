/*
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

#ifndef LOKI_DEST_HPP
#define LOKI_DEST_HPP

#include "loki-dest.h"

#include "compat/cpp-start.h"
#include "template/templates.h"
#include "stats/stats-cluster-key-builder.h"
#include "logmsg/logmsg.h"
#include "compat/cpp-end.h"

#include "credentials/grpc-credentials-builder.hpp"
#include "metrics/grpc-metrics.hpp"

#include <string>
#include <vector>
#include <memory>

namespace syslogng {
namespace grpc {
namespace loki {

struct Label
{
  std::string name;
  LogTemplate *value;

  Label(std::string name_, LogTemplate *value_)
    : name(name_), value(log_template_ref(value_)) {}

  Label(const Label &a)
    : name(a.name), value(log_template_ref(a.value)) {}

  Label &operator=(const Label &a)
  {
    name = a.name;
    log_template_unref(value);
    value = log_template_ref(a.value);

    return *this;
  }

  ~Label()
  {
    log_template_unref(value);
  }

};

class DestinationDriver final
{
public:
  DestinationDriver(LokiDestDriver *s);
  ~DestinationDriver();
  bool init();
  bool deinit();
  const gchar *format_persist_name();
  const gchar *format_stats_key(StatsClusterKeyBuilder *kb);
  GrpcClientCredentialsBuilderW *get_credentials_builder_wrapper();

  void add_label(std::string name, LogTemplate *value);

  LogTemplateOptions &get_template_options()
  {
    return this->template_options;
  }

  void set_url(std::string u)
  {
    this->url = u;
  }

  void set_message_template_ref(LogTemplate *msg)
  {
    log_template_unref(this->message);
    this->message = msg;
  }

  bool set_timestamp(const char *t)
  {
    if (strcasecmp(t, "current") == 0)
      this->timestamp = LM_TS_PROCESSED;
    else if (strcasecmp(t, "received") == 0)
      this->timestamp = LM_TS_RECVD;
    else if (strcasecmp(t, "msg") == 0)
      this->timestamp = LM_TS_STAMP;
    else
      return false;
    return true;
  }

  void set_keepalive_time(int t)
  {
    this->keepalive_time = t;
  }

  void set_keepalive_timeout(int t)
  {
    this->keepalive_timeout = t;
  }

  void set_keepalive_max_pings(int p)
  {
    this->keepalive_max_pings_without_data = p;
  }

  void set_tenant_id(std::string tid)
  {
    this->tenant_id = tid;
  }

  void add_extra_channel_arg(std::string name, long value)
  {
    this->int_extra_channel_args.push_back(std::pair<std::string, long> {name, value});
  }

  void add_extra_channel_arg(std::string name, std::string value)
  {
    this->string_extra_channel_args.push_back(std::pair<std::string, std::string> {name, value});
  }

  const std::string &get_url()
  {
    return this->url;
  }

private:
  friend class DestinationWorker;

private:
  LokiDestDriver *super;
  LogTemplateOptions template_options;

  std::string url;
  std::string tenant_id;

  LogTemplate *message = nullptr;
  std::vector<Label> labels;
  LogMessageTimeStamp timestamp;

  syslogng::grpc::ClientCredentialsBuilder credentials_builder;
  GrpcClientCredentialsBuilderW credentials_builder_wrapper;

  int keepalive_time;
  int keepalive_timeout;
  int keepalive_max_pings_without_data;

  std::list<std::pair<std::string, long>> int_extra_channel_args;
  std::list<std::pair<std::string, std::string>> string_extra_channel_args;

  DestDriverMetrics metrics;
};


}
}
}

syslogng::grpc::loki::DestinationDriver *loki_dd_get_cpp(LokiDestDriver *self);

#endif
