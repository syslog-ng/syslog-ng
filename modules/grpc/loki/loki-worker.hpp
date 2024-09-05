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

#ifndef LOKI_WORKER_HPP
#define LOKI_WORKER_HPP

#include "loki-dest.hpp"
#include "grpc-dest-worker.hpp"

#include "compat/cpp-start.h"
#include "messages.h"
#include "compat/cpp-end.h"

#include <grpcpp/create_channel.h>

#include <string>
#include <memory>

#include "push.grpc.pb.h"

namespace syslogng {
namespace grpc {
namespace loki {

class DestinationWorker final: public syslogng::grpc::DestWorker
{
public:
  DestinationWorker(GrpcDestWorker *s) : syslogng::grpc::DestWorker(s) {};
  ~DestinationWorker() {};

  bool init();
  bool connect();
  void disconnect();
  LogThreadedResult insert(LogMessage *msg);
  LogThreadedResult flush(LogThreadedFlushMode mode);

private:
  void prepare_batch();
  bool should_initiate_flush();
  void set_labels(LogMessage *msg);
  void set_timestamp(logproto::EntryAdapter *entry, LogMessage *msg);
  DestinationDriver *get_owner();

private:
  bool connected;

  std::shared_ptr<::grpc::Channel> channel;
  std::unique_ptr<logproto::Pusher::Stub> stub;
  logproto::PushRequest current_batch;
  size_t current_batch_bytes = 0;
};

}
}
}

#endif
