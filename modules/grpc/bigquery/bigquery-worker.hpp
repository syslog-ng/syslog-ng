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

#ifndef BIGQUERY_WORKER_HPP
#define BIGQUERY_WORKER_HPP

#include "bigquery-worker.h"
#include "bigquery-dest.hpp"

#include "compat/cpp-start.h"
#include "messages.h"
#include "compat/cpp-end.h"

#include <grpcpp/create_channel.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include <string>
#include <memory>
#include <cstddef>

#include "google/cloud/bigquery/storage/v1/storage.grpc.pb.h"

namespace syslogng {
namespace grpc {
namespace bigquery {

class DestinationWorker final
{
private:
  struct Slice
  {
    const char *str;
    std::size_t len;
  };

public:
  DestinationWorker(BigQueryDestWorker *s);
  ~DestinationWorker();

  bool init();
  void deinit();
  bool connect();
  void disconnect();
  LogThreadedResult insert(LogMessage *msg);
  LogThreadedResult flush(LogThreadedFlushMode mode);

private:
  std::shared_ptr<::grpc::Channel> create_channel();
  void construct_write_stream();
  void prepare_batch();
  bool should_initiate_flush();
  bool insert_field(const google::protobuf::Reflection *reflection, const Field &field,
                    LogMessage *msg, google::protobuf::Message *message);
  LogThreadedResult handle_row_errors(const google::cloud::bigquery::storage::v1::AppendRowsResponse &response);
  Slice format_template(LogTemplate *tmpl, LogMessage *msg, GString *value, LogMessageValueType *type);
  DestinationDriver *get_owner();

private:
  BigQueryDestWorker *super;

  std::string table;
  bool connected;

  std::shared_ptr<::grpc::Channel> channel;
  std::unique_ptr<google::cloud::bigquery::storage::v1::BigQueryWrite::Stub> stub;

  google::cloud::bigquery::storage::v1::WriteStream write_stream;
  std::unique_ptr<::grpc::ClientContext> batch_writer_ctx;
  std::unique_ptr<::grpc::ClientReaderWriter<google::cloud::bigquery::storage::v1::AppendRowsRequest,
      google::cloud::bigquery::storage::v1::AppendRowsResponse>> batch_writer;

  /* batch state */
  google::cloud::bigquery::storage::v1::AppendRowsRequest current_batch;
  size_t batch_size = 0;
  size_t current_batch_bytes = 0;
};

}
}
}

#endif
