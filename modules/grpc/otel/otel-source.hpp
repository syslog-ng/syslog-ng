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

#ifndef OTEL_SOURCE_HPP
#define OTEL_SOURCE_HPP

#include <grpcpp/server.h>

#include "compat/cpp-start.h"
#include "logthrsource/logthrsourcedrv.h"
#include "compat/cpp-end.h"

#include "otel-source.h"


namespace syslogng {
namespace grpc {
namespace otel {

class SourceLogsService;
class SourceMetricsService;
class SourceTraceService;

class SourceDriver
{
public:
  SourceDriver(OtelSourceDriver *s);

  void run();
  void request_exit();
  const gchar *format_stats_instance();
  gboolean init();
  gboolean deinit();

private:
  bool post(LogMessage *msg);

  friend class SourceLogsService;
  friend class SourceMetricsService;
  friend class SourceTraceService;

public:
  guint64 port = 4317;

private:
  OtelSourceDriver *super;
  std::unique_ptr<::grpc::Server> server;
};

}
}
}

struct OtelSourceDriver_
{
  LogThreadedSourceDriver super;
  syslogng::grpc::otel::SourceDriver *cpp;
};

#endif
