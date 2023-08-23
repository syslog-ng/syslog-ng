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
#include "syslog-ng.h"
#include "template/templates.h"
#include "stats/stats-cluster-key-builder.h"
#include "compat/cpp-end.h"

#include <string>
#include <memory>

namespace syslogng {
namespace grpc {
namespace loki {

class DestinationDriver final
{
public:
  DestinationDriver(LokiDestDriver *s);
  ~DestinationDriver();
  bool init();
  bool deinit();
  const gchar *format_persist_name();
  const gchar *format_stats_key(StatsClusterKeyBuilder *kb);

  LogTemplateOptions &get_template_options()
  {
    return this->template_options;
  }

  void set_url(std::string u)
  {
    this->url = u;
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

  int keepalive_time;
  int keepalive_timeout;
  int keepalive_max_pings_without_data;
};


}
}
}

syslogng::grpc::loki::DestinationDriver *loki_dd_get_cpp(LokiDestDriver *self);

#endif
