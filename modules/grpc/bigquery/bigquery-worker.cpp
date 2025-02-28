/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2023 László Várady
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

#include "bigquery-worker.hpp"
#include "bigquery-dest.hpp"

#include "compat/cpp-start.h"
#include "logthrdest/logthrdestdrv.h"
#include "scratch-buffers.h"
#include "logmsg/type-hinting.h"
#include "compat/cpp-end.h"

#include <string>
#include <sstream>
#include <chrono>

#include <grpc/grpc.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <google/protobuf/compiler/importer.h>

using syslogng::grpc::bigquery::DestinationWorker;
using syslogng::grpc::bigquery::DestinationDriver;
using google::protobuf::FieldDescriptor;

DestinationWorker::DestinationWorker(GrpcDestWorker *s)
  : syslogng::grpc::DestWorker(s)
{
  std::stringstream table_name;
  DestinationDriver *owner_ = this->get_owner();
  table_name << "projects/" << owner_->get_project()
             << "/datasets/" << owner_->get_dataset()
             << "/tables/" << owner_->get_table();
  this->table = table_name.str();
}

DestinationWorker::~DestinationWorker()
{
}

bool
DestinationWorker::connect()
{
  if (!this->channel)
    {
      this->channel = this->create_channel();
      if (!this->channel)
        return false;

      this->stub = google::cloud::bigquery::storage::v1::BigQueryWrite().NewStub(this->channel);
    }

  this->construct_write_stream();
  this->batch_writer_ctx = std::make_unique<::grpc::ClientContext>();
  this->prepare_context(*this->batch_writer_ctx.get());
  this->batch_writer = this->stub->AppendRows(this->batch_writer_ctx.get());

  this->prepare_batch();

  msg_debug("Connecting to BigQuery", log_pipe_location_tag((LogPipe *) this->super->super.owner));

  std::chrono::system_clock::time_point connect_timeout =
    std::chrono::system_clock::now() + std::chrono::seconds(10);

  if (!this->channel->WaitForConnected(connect_timeout))
    return false;

  this->connected = true;
  return true;
}

void
DestinationWorker::disconnect()
{
  if (!this->connected)
    return;

  if (!this->batch_writer->WritesDone())
    msg_warning("Error closing BigQuery write stream, writes may have been unsuccessful",
                log_pipe_location_tag((LogPipe *) this->super->super.owner));

  ::grpc::Status status = this->batch_writer->Finish();
  if (!status.ok() && status.error_code() != ::grpc::StatusCode::INVALID_ARGUMENT)
    {
      msg_warning("Error closing BigQuery stream", evt_tag_str("error", status.error_message().c_str()),
                  evt_tag_str("details", status.error_details().c_str()),
                  evt_tag_int("code", status.error_code()),
                  log_pipe_location_tag((LogPipe *) this->super->super.owner));
    }

  ::grpc::ClientContext ctx;
  this->prepare_context(ctx);
  google::cloud::bigquery::storage::v1::FinalizeWriteStreamRequest finalize_request;
  google::cloud::bigquery::storage::v1::FinalizeWriteStreamResponse finalize_response;
  finalize_request.set_name(write_stream.name());

  status = this->stub->FinalizeWriteStream(&ctx, finalize_request, &finalize_response);
  if (!status.ok())
    {
      msg_warning("Error finalizing BigQuery write stream", evt_tag_str("error", status.error_message().c_str()),
                  evt_tag_str("details", status.error_details().c_str()),
                  log_pipe_location_tag((LogPipe *) this->super->super.owner));
    }

  this->connected = false;
}

void
DestinationWorker::prepare_batch()
{
  this->batch_size = 0;
  this->current_batch_bytes = 0;
  this->current_batch = google::cloud::bigquery::storage::v1::AppendRowsRequest{};

  this->current_batch.set_write_stream(write_stream.name());
  this->current_batch.set_trace_id("syslog-ng-bigquery");
  google::cloud::bigquery::storage::v1::AppendRowsRequest_ProtoData *proto_rows =
    this->current_batch.mutable_proto_rows();
  google::cloud::bigquery::storage::v1::ProtoSchema *schema = proto_rows->mutable_writer_schema();
  this->get_owner()->schema_descriptor->CopyTo(schema->mutable_proto_descriptor());
}

bool
DestinationWorker::should_initiate_flush()
{
  return (this->current_batch_bytes >= this->get_owner()->batch_bytes);
}

DestinationWorker::Slice
DestinationWorker::format_template(LogTemplate *tmpl, LogMessage *msg, GString *value, LogMessageValueType *type)
{
  DestinationDriver *owner_ = this->get_owner();

  if (log_template_is_trivial(tmpl))
    {
      gssize trivial_value_len;
      const gchar *trivial_value = log_template_get_trivial_value_and_type(tmpl, msg, &trivial_value_len, type);

      if (trivial_value_len < 0)
        return Slice{"", 0};

      return Slice{trivial_value, (std::size_t) trivial_value_len};
    }

  LogTemplateEvalOptions options = {&owner_->template_options, LTZ_SEND, this->super->super.seq_num, NULL, LM_VT_STRING};
  log_template_format_value_and_type(tmpl, msg, &options, value, type);
  return Slice{value->str, value->len};
}

bool
DestinationWorker::insert_field(const google::protobuf::Reflection *reflection, const Field &field,
                                LogMessage *msg, google::protobuf::Message *message)
{
  DestinationDriver *owner_ = this->get_owner();

  ScratchBuffersMarker m;
  GString *buf = scratch_buffers_alloc_and_mark(&m);

  LogMessageValueType type;

  Slice value = this->format_template(field.nv.value, msg, buf, &type);

  if (type == LM_VT_NULL)
    {
      if (field.field_desc->is_required())
        {
          msg_error("Missing required field", evt_tag_str("field", field.nv.name.c_str()));
          goto error;
        }

      scratch_buffers_reclaim_marked(m);
      return true;
    }

  switch (field.field_desc->cpp_type())
    {
    /* TYPE_STRING, TYPE_BYTES (embedded nulls are possible, no null-termination is assumed) */
    case FieldDescriptor::CppType::CPPTYPE_STRING:
      reflection->SetString(message, field.field_desc, std::string{value.str, value.len});
      break;
    case FieldDescriptor::CppType::CPPTYPE_INT32:
    {
      int32_t v;
      if (!type_cast_to_int32(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(owner_->template_options.on_error, value.str, -1, "integer");
          goto error;
        }
      reflection->SetInt32(message, field.field_desc, v);
      break;
    }
    case FieldDescriptor::CppType::CPPTYPE_INT64:
    {
      gint64 v;
      if (!type_cast_to_int64(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(owner_->template_options.on_error, value.str, -1, "integer");
          goto error;
        }
      reflection->SetInt64(message, field.field_desc, v);
      break;
    }
    case FieldDescriptor::CppType::CPPTYPE_UINT32:
    {
      gint64 v;
      if (!type_cast_to_int64(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(owner_->template_options.on_error, value.str, -1, "integer");
          goto error;
        }
      reflection->SetUInt32(message, field.field_desc, (uint32_t) v);
      break;
    }
    case FieldDescriptor::CppType::CPPTYPE_UINT64:
    {
      gint64 v;
      if (!type_cast_to_int64(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(owner_->template_options.on_error, value.str, -1, "integer");
          goto error;
        }
      reflection->SetUInt64(message, field.field_desc, (uint64_t) v);
      break;
    }
    case FieldDescriptor::CppType::CPPTYPE_DOUBLE:
    {
      double v;
      if (!type_cast_to_double(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(owner_->template_options.on_error, value.str, -1, "double");
          goto error;
        }
      reflection->SetDouble(message, field.field_desc, v);
      break;
    }
    case FieldDescriptor::CppType::CPPTYPE_FLOAT:
    {
      double v;
      if (!type_cast_to_double(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(owner_->template_options.on_error, value.str, -1, "double");
          goto error;
        }
      reflection->SetFloat(message, field.field_desc, (float) v);
      break;
    }
    case FieldDescriptor::CppType::CPPTYPE_BOOL:
    {
      gboolean v;
      if (!type_cast_to_boolean(value.str, -1, &v, NULL))
        {
          type_cast_drop_helper(owner_->template_options.on_error, value.str, -1, "boolean");
          goto error;
        }
      reflection->SetBool(message, field.field_desc, v);
      break;
    }
    default:
      goto error;
    }

  scratch_buffers_reclaim_marked(m);
  return true;

error:
  scratch_buffers_reclaim_marked(m);
  return false;
}

LogThreadedResult
DestinationWorker::insert(LogMessage *msg)
{
  DestinationDriver *owner_ = this->get_owner();
  std::string serialized_row;
  size_t row_bytes = 0;

  google::cloud::bigquery::storage::v1::ProtoRows *rows = this->current_batch.mutable_proto_rows()->mutable_rows();

  google::protobuf::Message *message = owner_->schema_prototype->New();
  const google::protobuf::Reflection *reflection = message->GetReflection();

  bool msg_has_field = false;
  for (const auto &field : owner_->fields)
    {
      bool field_inserted = this->insert_field(reflection, field, msg, message);
      msg_has_field |= field_inserted;

      if (!field_inserted && (owner_->template_options.on_error & ON_ERROR_DROP_MESSAGE))
        goto drop;
    }

  if (!msg_has_field)
    goto drop;

  this->batch_size++;

  message->SerializePartialToString(&serialized_row);
  row_bytes = serialized_row.size();
  rows->add_serialized_rows(std::move(serialized_row));

  this->current_batch_bytes += row_bytes;
  log_threaded_dest_driver_insert_msg_length_stats(this->super->super.owner, row_bytes);

  msg_trace("Message added to BigQuery batch", log_pipe_location_tag((LogPipe *) this->super->super.owner));

  delete message;

  if (this->should_initiate_flush())
    return log_threaded_dest_worker_flush(&this->super->super, LTF_FLUSH_NORMAL);

  return LTR_QUEUED;

drop:
  if (!(owner_->template_options.on_error & ON_ERROR_SILENT))
    {
      msg_error("Failed to format message for BigQuery, dropping message",
                log_pipe_location_tag((LogPipe *) this->super->super.owner));
    }
  delete message;

  /* LTR_DROP currently drops the entire batch */
  return LTR_QUEUED;
}

LogThreadedResult
DestinationWorker::handle_row_errors(const google::cloud::bigquery::storage::v1::AppendRowsResponse &response)
{
  for (const auto &error : response.row_errors())
    {
      msg_error("BigQuery row error",
                evt_tag_int("index", error.index()),
                evt_tag_str("error", error.message().c_str()),
                evt_tag_int("code", error.code()),
                log_pipe_location_tag((LogPipe *) this->super->super.owner));
    }

  return LTR_DROP;
}

static ::grpc::Status
_append_rows_response_get_status(const google::cloud::bigquery::storage::v1::AppendRowsResponse &response)
{
  if (!response.has_error())
    return ::grpc::Status::OK;

  return ::grpc::Status((::grpc::StatusCode) response.error().code(), response.error().message());
}

LogThreadedResult
DestinationWorker::flush(LogThreadedFlushMode mode)
{
  if (this->batch_size == 0)
    return LTR_SUCCESS;

  LogThreadedResult result;
  google::cloud::bigquery::storage::v1::AppendRowsResponse append_rows_response;

  if (!this->batch_writer->Write(current_batch))
    {
      msg_error("Error writing BigQuery batch", log_pipe_location_tag((LogPipe *) this->super->super.owner));
      result = LTR_ERROR;
      goto exit;
    }

  if (!this->batch_writer->Read(&append_rows_response))
    {
      msg_error("Error reading BigQuery batch response", log_pipe_location_tag((LogPipe *) this->super->super.owner));
      result = LTR_ERROR;
      goto exit;
    }

  if (append_rows_response.has_error() && append_rows_response.error().code() != ::grpc::StatusCode::ALREADY_EXISTS)
    {
      msg_error("Error in BigQuery batch",
                evt_tag_str("error", append_rows_response.error().message().c_str()),
                evt_tag_int("code", append_rows_response.error().code()),
                log_pipe_location_tag((LogPipe *) this->super->super.owner));

      result = LTR_ERROR;

      if (append_rows_response.row_errors_size() != 0)
        result = handle_row_errors(append_rows_response);

      goto exit;
    }

  log_threaded_dest_worker_written_bytes_add(&this->super->super, this->current_batch_bytes);
  log_threaded_dest_driver_insert_batch_length_stats(this->super->super.owner, this->current_batch_bytes);

  msg_debug("BigQuery batch delivered", log_pipe_location_tag((LogPipe *) this->super->super.owner));
  result = LTR_SUCCESS;

exit:
  this->get_owner()->metrics.insert_grpc_request_stats(_append_rows_response_get_status(append_rows_response));
  this->prepare_batch();
  return result;
}

std::shared_ptr<::grpc::Channel>
DestinationWorker::create_channel()
{
  DestinationDriver *owner_ = this->get_owner();

  ::grpc::ChannelArguments args = this->create_channel_args();
  auto credentials = this->create_credentials();
  if (!credentials)
    {
      msg_error("Error querying BigQuery credentials", log_pipe_location_tag((LogPipe *) this->super->super.owner));
      return nullptr;
    }

  auto channel_ = ::grpc::CreateCustomChannel(owner_->get_url(), credentials, args);
  if (!channel_)
    {
      msg_error("Error creating BigQuery gRPC channel", log_pipe_location_tag((LogPipe *) this->super->super.owner));
      return nullptr;
    }

  return channel_;
}

void
DestinationWorker::construct_write_stream()
{
  ::grpc::ClientContext ctx;
  this->prepare_context(ctx);
  google::cloud::bigquery::storage::v1::CreateWriteStreamRequest create_write_stream_request;
  google::cloud::bigquery::storage::v1::WriteStream wstream;

  create_write_stream_request.set_parent(this->table);
  create_write_stream_request.mutable_write_stream()->set_type(
    google::cloud::bigquery::storage::v1::WriteStream_Type_COMMITTED);

  stub->CreateWriteStream(&ctx, create_write_stream_request, &wstream);

  this->write_stream = wstream;
}

DestinationDriver *
DestinationWorker::get_owner()
{
  return bigquery_dd_get_cpp(this->owner.super);
}
