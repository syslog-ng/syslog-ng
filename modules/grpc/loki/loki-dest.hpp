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

#ifndef LOKI_DEST_HPP
#define LOKI_DEST_HPP

#include "loki-dest.h"

#include "compat/cpp-start.h"
#include "template/templates.h"
#include "stats/stats-cluster-key-builder.h"
#include "logmsg/logmsg.h"
#include "compat/cpp-end.h"

#include "grpc-dest.hpp"

#include <string>
#include <vector>
#include <memory>

namespace syslogng {
namespace grpc {
namespace loki {

class DestinationDriver final : public syslogng::grpc::DestDriver
{
public:
  DestinationDriver(GrpcDestDriver *s);
  ~DestinationDriver();
  bool init();
  const gchar *generate_persist_name();
  const gchar *format_stats_key(StatsClusterKeyBuilder *kb);
  LogThreadedDestWorker *construct_worker(int worker_index);

  void add_label(std::string name, LogTemplate *value);

  LogTemplateOptions &get_template_options()
  {
    return this->template_options;
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

  void set_tenant_id(std::string tid)
  {
    this->tenant_id = tid;
  }

private:
  friend class DestinationWorker;
  LogTemplateOptions template_options;

  std::string tenant_id;

  LogTemplate *message = nullptr;
  std::vector<NameValueTemplatePair> labels;
  LogMessageTimeStamp timestamp;
};


}
}
}

syslogng::grpc::loki::DestinationDriver *loki_dd_get_cpp(GrpcDestDriver *self);

#endif
