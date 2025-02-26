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

#ifndef GRPC_SOURCE_WORKER_HPP
#define GRPC_SOURCE_WORKER_HPP

#include "grpc-source.hpp"

typedef struct GrpcSourceWorker_ GrpcSourceWorker;

namespace syslogng {
namespace grpc {

class SourceWorker
{
public:
  SourceWorker(GrpcSourceWorker *s, SourceDriver &d);
  virtual ~SourceWorker() {};

  virtual void run() = 0;
  virtual void request_exit() = 0;
  void post(LogMessage *msg);

public:
  GrpcSourceWorker *super;
  SourceDriver &driver;
};

}
}

GrpcSourceWorker *grpc_sw_new(GrpcSourceDriver *o, gint worker_index);

struct GrpcSourceWorker_
{
  LogThreadedSourceWorker super;
  syslogng::grpc::SourceWorker *cpp;
};

#endif
