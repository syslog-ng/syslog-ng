/*
 * Copyright (c) 2023 László Várady
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

struct _BigQueryDestWorker
{
  LogThreadedDestWorker super;
  DestinationWorker *cpp;
};

DestinationWorker::DestinationWorker(BigQueryDestWorker *s) : super(s)
{
  DestinationDriver *owner = this->get_owner();

  std::stringstream table_name;
  table_name << "projects/" << owner->get_project()
             << "/datasets/" << owner->get_dataset()
             << "/tables/" << owner->get_table();
  this->table = table_name.str();
}

DestinationWorker::~DestinationWorker()
{
}

bool
DestinationWorker::init()
{
  DestinationDriver *owner = this->get_owner();

  ::grpc::ChannelArguments args{};

  if (owner->keepalive_time != -1)
    args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, owner->keepalive_time);
  if (owner->keepalive_timeout != -1)
    args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, owner->keepalive_timeout);
  if (owner->keepalive_max_pings_without_data != -1)
    args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, owner->keepalive_max_pings_without_data);

  args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);

  auto credentials = ::grpc::GoogleDefaultCredentials();
  if (!credentials)
    {
      msg_error("Error querying BigQuery credentials", log_pipe_location_tag((LogPipe *) this->super->super.owner));
      return false;
    }

  this->channel = ::grpc::CreateCustomChannel(owner->get_url(), credentials, args);
  if (!this->channel)
    {
      msg_error("Error creating BigQuery gRPC channel", log_pipe_location_tag((LogPipe *) this->super->super.owner));
      return false;
    }

  this->stub = google::cloud::bigquery::storage::v1::BigQueryWrite().NewStub(channel);

  return log_threaded_dest_worker_init_method(&this->super->super);
}

void
DestinationWorker::deinit()
{
  log_threaded_dest_worker_deinit_method(&this->super->super);
}

bool
DestinationWorker::connect()
{
  this->construct_write_stream();
  this->batch_writer_ctx = std::make_unique<::grpc::ClientContext>();
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
  this->current_batch = google::cloud::bigquery::storage::v1::AppendRowsRequest{};

  this->current_batch.set_write_stream(write_stream.name());
  this->current_batch.set_trace_id("syslog-ng-bigquery");
  google::cloud::bigquery::storage::v1::AppendRowsRequest_ProtoData *proto_rows =
    this->current_batch.mutable_proto_rows();
  google::cloud::bigquery::storage::v1::ProtoSchema *schema = proto_rows->mutable_writer_schema();
  this->get_owner()->schema_descriptor->CopyTo(schema->mutable_proto_descriptor());
}

void
DestinationWorker::format_template(LogTemplate *tmpl, LogMessage *msg, GString *value, LogMessageValueType *type)
{
  DestinationDriver *owner = this->get_owner();

  if (log_template_is_trivial(tmpl))
    {
      gssize trivial_value_len;
      const gchar *trivial_value = log_template_get_trivial_value_and_type(tmpl, msg, &trivial_value_len, type);

      g_string_truncate(value, 0);
      if (trivial_value)
        g_string_append_len(value, trivial_value, trivial_value_len);

      return;
    }

  LogTemplateEvalOptions options = {&owner->template_options, LTZ_SEND, this->super->super.seq_num, NULL, LM_VT_STRING};
  log_template_format_value_and_type(tmpl, msg, &options, value, type);
}

bool
DestinationWorker::insert_field(const google::protobuf::Reflection *reflection, const Field &field,
                                LogMessage *msg, google::protobuf::Message *message)
{
  DestinationDriver *owner = this->get_owner();

  ScratchBuffersMarker m;
  GString *value = scratch_buffers_alloc_and_mark(&m);

  LogMessageValueType type;

  this->format_template(field.value, msg, value, &type);

  if (type == LM_VT_NULL)
    {
      if (field.field_desc->is_required())
        {
          msg_error("Missing required field", evt_tag_str("field", field.name.c_str()));
          goto error;
        }

      scratch_buffers_reclaim_marked(m);
      return true;
    }

  switch (field.field_desc->cpp_type())
    {
    /* TYPE_STRING, TYPE_BYTES (embedded nulls are possible, no null-termination is assumed) */
    case FieldDescriptor::CppType::CPPTYPE_STRING:
      reflection->SetString(message, field.field_desc, std::string{value->str, value->len});
      break;
    case FieldDescriptor::CppType::CPPTYPE_INT32:
    {
      int32_t v;
      if (!type_cast_to_int32(value->str, &v, NULL))
        {
          type_cast_drop_helper(owner->template_options.on_error, value->str, "integer");
          goto error;
        }
      reflection->SetInt32(message, field.field_desc, v);
      break;
    }
    case FieldDescriptor::CppType::CPPTYPE_INT64:
    {
      int64_t v;
      if (!type_cast_to_int64(value->str, &v, NULL))
        {
          type_cast_drop_helper(owner->template_options.on_error, value->str, "integer");
          goto error;
        }
      reflection->SetInt64(message, field.field_desc, v);
      break;
    }
    case FieldDescriptor::CppType::CPPTYPE_UINT32:
    {
      int64_t v;
      if (!type_cast_to_int64(value->str, &v, NULL))
        {
          type_cast_drop_helper(owner->template_options.on_error, value->str, "integer");
          goto error;
        }
      reflection->SetUInt32(message, field.field_desc, (uint32_t) v);
      break;
    }
    case FieldDescriptor::CppType::CPPTYPE_UINT64:
    {
      int64_t v;
      if (!type_cast_to_int64(value->str, &v, NULL))
        {
          type_cast_drop_helper(owner->template_options.on_error, value->str, "integer");
          goto error;
        }
      reflection->SetUInt64(message, field.field_desc, (uint64_t) v);
      break;
    }
    case FieldDescriptor::CppType::CPPTYPE_DOUBLE:
    {
      double v;
      if (!type_cast_to_double(value->str, &v, NULL))
        {
          type_cast_drop_helper(owner->template_options.on_error, value->str, "double");
          goto error;
        }
      reflection->SetDouble(message, field.field_desc, v);
      break;
    }
    case FieldDescriptor::CppType::CPPTYPE_FLOAT:
    {
      double v;
      if (!type_cast_to_double(value->str, &v, NULL))
        {
          type_cast_drop_helper(owner->template_options.on_error, value->str, "double");
          goto error;
        }
      reflection->SetFloat(message, field.field_desc, (float) v);
      break;
    }
    case FieldDescriptor::CppType::CPPTYPE_BOOL:
    {
      gboolean v;
      if (!type_cast_to_boolean(value->str, &v, NULL))
        {
          type_cast_drop_helper(owner->template_options.on_error, value->str, "boolean");
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
  DestinationDriver *owner = this->get_owner();

  google::cloud::bigquery::storage::v1::ProtoRows *rows = this->current_batch.mutable_proto_rows()->mutable_rows();

  google::protobuf::Message *message = owner->schema_prototype->New();
  const google::protobuf::Reflection *reflection = message->GetReflection();

  bool msg_has_field = false;
  for (const auto &field : owner->fields)
    {
      bool field_inserted = this->insert_field(reflection, field, msg, message);
      msg_has_field |= field_inserted;

      if (!field_inserted && (owner->template_options.on_error & ON_ERROR_DROP_MESSAGE))
        goto drop;
    }

  if (!msg_has_field)
    goto drop;

  this->batch_size++;
  rows->add_serialized_rows(message->SerializePartialAsString());

  msg_trace("Message added to BigQuery batch", log_pipe_location_tag((LogPipe *) this->super->super.owner));

  delete message;
  return LTR_QUEUED;

drop:
  if (!(owner->template_options.on_error & ON_ERROR_SILENT))
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

  msg_debug("BigQuery batch delivered", log_pipe_location_tag((LogPipe *) this->super->super.owner));
  result = LTR_SUCCESS;

exit:
  this->prepare_batch();
  return result;
}

void
DestinationWorker::construct_write_stream()
{
  ::grpc::ClientContext ctx;
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
  return bigquery_dd_get_cpp((BigQueryDestDriver *) this->super->super.owner);
}

/* C Wrappers */

static LogThreadedResult
_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  BigQueryDestWorker *self = (BigQueryDestWorker *) s;
  return self->cpp->insert(msg);
}

static LogThreadedResult
_flush(LogThreadedDestWorker *s, LogThreadedFlushMode mode)
{
  BigQueryDestWorker *self = (BigQueryDestWorker *) s;
  return self->cpp->flush(mode);
}

static gboolean
_connect(LogThreadedDestWorker *s)
{
  BigQueryDestWorker *self = (BigQueryDestWorker *) s;
  return self->cpp->connect();
}

static void
_disconnect(LogThreadedDestWorker *s)
{
  BigQueryDestWorker *self = (BigQueryDestWorker *) s;
  self->cpp->disconnect();
}

static gboolean
_init(LogThreadedDestWorker *s)
{
  BigQueryDestWorker *self = (BigQueryDestWorker *) s;
  return self->cpp->init();
}

static void
_deinit(LogThreadedDestWorker *s)
{
  BigQueryDestWorker *self = (BigQueryDestWorker *) s;
  self->cpp->deinit();
}

static void
_free(LogThreadedDestWorker *s)
{
  BigQueryDestWorker *self = (BigQueryDestWorker *) s;
  delete self->cpp;

  log_threaded_dest_worker_free_method(s);
}

LogThreadedDestWorker *
bigquery_dw_new(LogThreadedDestDriver *o, gint worker_index)
{
  BigQueryDestWorker *self = g_new0(BigQueryDestWorker, 1);

  log_threaded_dest_worker_init_instance(&self->super, o, worker_index);

  self->cpp = new DestinationWorker(self);

  self->super.init = _init;
  self->super.deinit = _deinit;
  self->super.connect = _connect;
  self->super.disconnect = _disconnect;
  self->super.insert = _insert;
  self->super.flush = _flush;
  self->super.free_fn = _free;

  return &self->super;
}
