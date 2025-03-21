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

#include "pubsub-dest-worker.hpp"
#include "pubsub-dest.hpp"

#include "compat/cpp-start.h"
#include "scratch-buffers.h"
#include "compat/cpp-end.h"

using syslogng::grpc::pubsub::DestWorker;
using syslogng::grpc::pubsub::DestDriver;

DestWorker::DestWorker(GrpcDestWorker *s)
  : syslogng::grpc::DestWorker(s)
{
  std::shared_ptr<::grpc::ChannelCredentials> credentials = this->create_credentials();
  if (!credentials)
    {
      msg_error("Error querying Google Pub/Sub credentials",
                evt_tag_str("url", this->owner.get_url().c_str()),
                log_pipe_location_tag(&this->super->super.owner->super.super.super));
      throw std::runtime_error("Error querying Google Pub/Sub credentials");
    }

  ::grpc::ChannelArguments args = this->create_channel_args();

  this->channel = ::grpc::CreateCustomChannel(this->owner.get_url(), credentials, args);
  this->stub = ::google::pubsub::v1::Publisher::NewStub(this->channel);
}

bool
DestWorker::should_initiate_flush()
{
  return this->current_batch_bytes >= this->get_owner()->batch_bytes;
}

const std::string
DestWorker::format_topic(LogMessage *msg)
{
  ScratchBuffersMarker m;
  scratch_buffers_mark(&m);

  GString *project_buf = scratch_buffers_alloc();
  GString *topic_buf = scratch_buffers_alloc();

  Slice project_slice = this->format_template(this->get_owner()->project, msg, project_buf, NULL, 0);
  Slice topic_slice = this->format_template(this->get_owner()->topic, msg, topic_buf, NULL, 0);

  std::string topic = std::string("projects/") + project_slice.str + "/topics/" + topic_slice.str;

  scratch_buffers_reclaim_marked(m);
  return topic;
}

/* TODO: create a C++ Template class, usefor all gRPC drivers. */
DestWorker::Slice
DestWorker::format_template(LogTemplate *tmpl, LogMessage *msg, GString *value, LogMessageValueType *type,
                            gint seq_num) const
{
  if (log_template_is_trivial(tmpl))
    {
      gssize trivial_value_len;
      const gchar *trivial_value = log_template_get_trivial_value_and_type(tmpl, msg, &trivial_value_len, type);

      if (trivial_value_len < 0)
        return Slice{"", 0};

      return Slice{trivial_value, (std::size_t) trivial_value_len};
    }

  LogTemplateEvalOptions options = {&this->owner.get_template_options(), LTZ_SEND, seq_num, NULL, LM_VT_STRING};
  log_template_format_value_and_type(tmpl, msg, &options, value, type);
  return Slice{value->str, value->len};
}

LogThreadedResult
DestWorker::insert(LogMessage *msg)
{
  DestDriver *owner_ = this->get_owner();

  ScratchBuffersMarker m;
  GString *buf = scratch_buffers_alloc_and_mark(&m);
  Slice buf_slice;
  size_t message_bytes = 0;

  ::google::pubsub::v1::PubsubMessage *message = this->request.add_messages();

  buf_slice = this->format_template(owner_->data, msg, buf, NULL, this->super->super.seq_num);
  message->set_data(buf_slice.str, buf_slice.len);
  message_bytes += buf_slice.len;

  auto attributes = message->mutable_attributes();
  for (const auto &attribute : owner_->attributes)
    {
      buf_slice = this->format_template(attribute.value, msg, buf, NULL, this->super->super.seq_num);
      attributes->insert({attribute.name, buf_slice.str});
      message_bytes += buf_slice.len;
    }

  scratch_buffers_reclaim_marked(m);

  this->current_batch_bytes += message_bytes;
  log_threaded_dest_driver_insert_msg_length_stats(this->super->super.owner, message_bytes);

  this->batch_size++;

  if (!this->client_context.get())
    {
      this->client_context = std::make_unique<::grpc::ClientContext>();
      prepare_context_dynamic(*this->client_context, msg);
      this->request.set_topic(this->format_topic(msg));
    }

  msg_trace("Message added to Google Pub/Sub batch",
            evt_tag_str("project/topic", this->request.topic().c_str()),
            log_pipe_location_tag(&this->super->super.owner->super.super.super));

  if (this->should_initiate_flush())
    return log_threaded_dest_worker_flush(&this->super->super, LTF_FLUSH_NORMAL);

  return LTR_QUEUED;
}

static LogThreadedResult
_map_grpc_status_to_log_threaded_result(const ::grpc::Status &status)
{
  // TODO: this is based on OTLP, we should check how the Google Pub/Sub gRPC server behaves

  switch (status.error_code())
    {
    case ::grpc::StatusCode::OK:
      return LTR_SUCCESS;
    case ::grpc::StatusCode::UNAVAILABLE:
    case ::grpc::StatusCode::CANCELLED:
    case ::grpc::StatusCode::DEADLINE_EXCEEDED:
    case ::grpc::StatusCode::ABORTED:
    case ::grpc::StatusCode::OUT_OF_RANGE:
    case ::grpc::StatusCode::DATA_LOSS:
      goto temporary_error;
    case ::grpc::StatusCode::UNKNOWN:
    case ::grpc::StatusCode::INVALID_ARGUMENT:
    case ::grpc::StatusCode::NOT_FOUND:
    case ::grpc::StatusCode::ALREADY_EXISTS:
    case ::grpc::StatusCode::PERMISSION_DENIED:
    case ::grpc::StatusCode::UNAUTHENTICATED:
    case ::grpc::StatusCode::FAILED_PRECONDITION:
    case ::grpc::StatusCode::UNIMPLEMENTED:
    case ::grpc::StatusCode::INTERNAL:
      goto permanent_error;
    case ::grpc::StatusCode::RESOURCE_EXHAUSTED:
      if (status.error_details().length() > 0)
        goto temporary_error;
      goto permanent_error;
    default:
      g_assert_not_reached();
    }

temporary_error:
  msg_debug("Google Pub/Sub server responded with a temporary error status code, retrying after time-reopen() seconds",
            evt_tag_int("error_code", status.error_code()),
            evt_tag_str("error_message", status.error_message().c_str()),
            evt_tag_str("error_details", status.error_details().c_str()));
  return LTR_NOT_CONNECTED;

permanent_error:
  msg_error("Google Pub/Sub server responded with a permanent error status code, dropping batch",
            evt_tag_int("error_code", status.error_code()),
            evt_tag_str("error_message", status.error_message().c_str()),
            evt_tag_str("error_details", status.error_details().c_str()));
  return LTR_DROP;
}

void
DestWorker::prepare_batch()
{
  this->request.clear_topic();
  this->request.clear_messages();
  this->batch_size = 0;
  this->current_batch_bytes = 0;
  this->client_context.reset();
}

LogThreadedResult
DestWorker::flush(LogThreadedFlushMode mode)
{
  if (this->batch_size == 0)
    return LTR_SUCCESS;

  ::google::pubsub::v1::PublishResponse response;

  ::grpc::Status status = this->stub->Publish(this->client_context.get(), this->request, &response);
  LogThreadedResult result = _map_grpc_status_to_log_threaded_result(status);
  if (result != LTR_SUCCESS)
    goto exit;

  log_threaded_dest_worker_written_bytes_add(&this->super->super, this->current_batch_bytes);
  log_threaded_dest_driver_insert_batch_length_stats(this->super->super.owner, this->current_batch_bytes);

  msg_debug("Google Pub/Sub batch delivered",
            evt_tag_str("project/topic", this->request.topic().c_str()),
            log_pipe_location_tag(&this->super->super.owner->super.super.super));

exit:
  this->get_owner()->metrics.insert_grpc_request_stats(status);
  this->prepare_batch();
  return result;
}

DestDriver *
DestWorker::get_owner()
{
  return pubsub_dd_get_cpp(this->owner.super);
}
