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

#ifndef SYSLOG_NG_OTLP_DEST_WORKER_HPP
#define SYSLOG_NG_OTLP_DEST_WORKER_HPP

#include "syslog-ng-otlp-dest.hpp"
#include "otel-dest-worker.hpp"

typedef OtelDestWorker SyslogNgOtlpDestWorker;

namespace syslogng {
namespace grpc {
namespace otel {

class SyslogNgDestWorker : public DestWorker
{
public:
  using DestWorker::DestWorker;
  static LogThreadedDestWorker *construct(LogThreadedDestDriver *o, gint worker_index);

  ScopeLogs *lookup_scope_logs(LogMessage *msg) override;
  LogThreadedResult insert(LogMessage *msg) override;
};

}
}
}

#endif
