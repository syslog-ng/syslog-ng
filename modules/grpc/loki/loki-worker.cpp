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

#include "loki-worker.hpp"
#include "loki-dest.hpp"

#include "compat/cpp-start.h"
#include "logthrdest/logthrdestdrv.h"
#include "scratch-buffers.h"
#include "logmsg/type-hinting.h"
#include "utf8utils.h"
#include "logmsg/logmsg.h"
#include "compat/cpp-end.h"

#include "push.grpc.pb.h"

#include <string>
#include <sstream>
#include <chrono>
#include <sys/time.h>

#include <grpc/grpc.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <google/protobuf/util/time_util.h>

using syslogng::grpc::loki::DestinationWorker;
using syslogng::grpc::loki::DestinationDriver;
using google::protobuf::FieldDescriptor;

struct _LokiDestWorker
{
  LogThreadedDestWorker super;
  DestinationWorker *cpp;
};

bool
DestinationWorker::init()
{
  ::grpc::ChannelArguments args = this->create_channel_args();
  auto credentials = this->create_credentials();
  if (!credentials)
    {
      msg_error("Error querying Loki credentials",
                evt_tag_str("url", this->owner.get_url().c_str()),
                log_pipe_location_tag((LogPipe *) this->super->super.owner));
      return false;
    }

  this->channel = ::grpc::CreateCustomChannel(this->owner.get_url(), credentials, args);
  if (!this->channel)
    {
      msg_error("Error creating Loki gRPC channel",
                evt_tag_str("url", this->owner.get_url().c_str()),
                log_pipe_location_tag((LogPipe *) this->super->super.owner));
      return false;
    }

  this->stub = logproto::Pusher().NewStub(channel);

  return syslogng::grpc::DestWorker::init();
}

bool
DestinationWorker::connect()
{
  DestinationDriver *owner_ = this->get_owner();

  this->prepare_batch();

  msg_debug("Connecting to Loki", log_pipe_location_tag((LogPipe *) this->super->super.owner));

  std::chrono::system_clock::time_point connect_timeout =
    std::chrono::system_clock::now() + std::chrono::seconds(10);

  if (!this->channel->WaitForConnected(connect_timeout))
    {
      msg_error("Time out connecting to Loki",
                evt_tag_str("url", owner_->get_url().c_str()),
                log_pipe_location_tag((LogPipe *) this->super->super.owner));
      return false;
    }

  this->connected = true;
  return true;
}

void
DestinationWorker::disconnect()
{
  if (!this->connected)
    return;

  this->connected = false;
}

void
DestinationWorker::prepare_batch()
{
  this->current_batch = logproto::PushRequest{};
  this->current_batch.add_streams();
  this->current_batch_bytes = 0;
  this->client_context.reset();
}

bool
DestinationWorker::should_initiate_flush()
{
  return (this->current_batch_bytes >= this->get_owner()->batch_bytes);
}

void
DestinationWorker::set_labels(LogMessage *msg)
{
  DestinationDriver *owner_ = this->get_owner();
  logproto::StreamAdapter *stream = this->current_batch.mutable_streams(0);

  LogTemplateEvalOptions options = {&owner_->template_options, LTZ_SEND, this->super->super.seq_num, NULL, LM_VT_STRING};

  ScratchBuffersMarker m;
  GString *buf = scratch_buffers_alloc_and_mark(&m);
  GString *sanitized_value = scratch_buffers_alloc();

  std::stringstream formatted_labels;
  bool comma_needed = false;
  formatted_labels << "{";
  for (const auto &label : owner_->labels)
    {
      if (comma_needed)
        formatted_labels << ", ";

      log_template_format(label.value, msg, &options, buf);

      g_string_truncate(sanitized_value, 0);
      append_unsafe_utf8_as_escaped_binary(sanitized_value, buf->str, -1, AUTF8_UNSAFE_QUOTE);

      formatted_labels << label.name << "=\"" << sanitized_value->str << "\"";

      comma_needed = true;
    }
  formatted_labels << "}";
  stream->set_labels(formatted_labels.str());

  scratch_buffers_reclaim_marked(m);
}

void
DestinationWorker::set_timestamp(logproto::EntryAdapter *entry, LogMessage *msg)
{
  DestinationDriver *owner_ = this->get_owner();

  if (owner_->timestamp == LM_TS_PROCESSED)
    {
      *entry->mutable_timestamp() = google::protobuf::util::TimeUtil::GetCurrentTime();
      return;
    }

  UnixTime *time = &msg->timestamps[owner_->timestamp];
  struct timeval tv = timeval_from_unix_time(time);
  *entry->mutable_timestamp() = google::protobuf::util::TimeUtil::TimevalToTimestamp(tv);
}

LogThreadedResult
DestinationWorker::insert(LogMessage *msg)
{
  DestinationDriver *owner_ = this->get_owner();
  logproto::StreamAdapter *stream = this->current_batch.mutable_streams(0);

  if (stream->entries_size() == 0)
    this->set_labels(msg);

  logproto::EntryAdapter *entry = stream->add_entries();

  this->set_timestamp(entry, msg);

  ScratchBuffersMarker m;
  GString *message = scratch_buffers_alloc_and_mark(&m);

  LogTemplateEvalOptions options = {&owner_->template_options, LTZ_SEND, this->super->super.seq_num, NULL, LM_VT_STRING};
  log_template_format(owner_->message, msg, &options, message);

  entry->set_line(message->str);
  scratch_buffers_reclaim_marked(m);

  this->current_batch_bytes += message->len;
  log_threaded_dest_driver_insert_msg_length_stats(super->super.owner, message->len);

  if (!this->client_context.get())
    {
      this->client_context = std::make_unique<::grpc::ClientContext>();
      this->prepare_context_dynamic(*this->client_context, msg);
      if (!owner_->tenant_id.empty())
        client_context->AddMetadata("x-scope-orgid", owner_->tenant_id);
    }

  msg_trace("Message added to Loki batch", log_pipe_location_tag((LogPipe *) this->super->super.owner));

  if (this->should_initiate_flush())
    return log_threaded_dest_worker_flush(&this->super->super, LTF_FLUSH_NORMAL);

  return LTR_QUEUED;
}

LogThreadedResult
DestinationWorker::flush(LogThreadedFlushMode mode)
{
  DestinationDriver *owner_ = this->get_owner();

  if (this->super->super.batch_size == 0)
    return LTR_SUCCESS;

  LogThreadedResult result;
  logproto::PushResponse response{};

  ::grpc::Status status = this->stub->Push(client_context.get(), this->current_batch, &response);
  this->get_owner()->metrics.insert_grpc_request_stats(status);

  if (this->get_owner()->handle_response(status, &result))
    {
      if (result == LTR_SUCCESS)
        goto success;
      goto error;
    }

  if (!status.ok())
    {
      msg_error("Error sending Loki batch", evt_tag_str("error", status.error_message().c_str()),
                evt_tag_str("url", owner_->get_url().c_str()),
                evt_tag_str("details", status.error_details().c_str()),
                log_pipe_location_tag((LogPipe *) this->super->super.owner));
      result = LTR_ERROR;
      goto error;
    }

success:
  log_threaded_dest_worker_written_bytes_add(&this->super->super, this->current_batch_bytes);
  log_threaded_dest_driver_insert_batch_length_stats(this->super->super.owner, this->current_batch_bytes);

  msg_debug("Loki batch delivered", log_pipe_location_tag((LogPipe *) this->super->super.owner));
  result = LTR_SUCCESS;

error:
  this->prepare_batch();
  return result;
}

DestinationDriver *
DestinationWorker::get_owner()
{
  return loki_dd_get_cpp(this->owner.super);
}
