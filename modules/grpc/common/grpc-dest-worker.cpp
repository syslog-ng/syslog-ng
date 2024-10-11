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

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "grpc-dest-worker.hpp"

#include "compat/cpp-start.h"
#include "scratch-buffers.h"
#include "compat/cpp-end.h"

using namespace syslogng::grpc;

/* C++ Implementations */

DestWorker::DestWorker(GrpcDestWorker *s)
  : super(s),
    owner(*(reinterpret_cast<GrpcDestDriver *>(s->super.owner))->cpp)
{
}

std::shared_ptr<::grpc::ChannelCredentials>
DestWorker::create_credentials()
{
  return this->owner.credentials_builder.build();
}

::grpc::ChannelArguments
DestWorker::create_channel_args()
{
  ::grpc::ChannelArguments args;

  if (this->owner.keepalive_time != -1)
    args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, this->owner.keepalive_time);
  if (this->owner.keepalive_timeout != -1)
    args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, this->owner.keepalive_timeout);
  if (this->owner.keepalive_max_pings_without_data != -1)
    args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, this->owner.keepalive_max_pings_without_data);

  if (this->owner.compression)
    args.SetCompressionAlgorithm(GRPC_COMPRESS_GZIP);

  args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);

  for (auto nv : this->owner.int_extra_channel_args)
    args.SetInt(nv.first, nv.second);
  for (auto nv : this->owner.string_extra_channel_args)
    args.SetString(nv.first, nv.second);

  return args;
}

bool
DestWorker::init()
{
  return log_threaded_dest_worker_init_method(&super->super);
}

void
DestWorker::deinit()
{
  log_threaded_dest_worker_deinit_method(&super->super);
}

bool
DestWorker::connect()
{
  return true;
}

void
DestWorker::disconnect()
{
}

void
DestWorker::prepare_context(::grpc::ClientContext &context)
{
  g_assert(!this->owner.dynamic_headers_enabled);

  for (auto nv : owner.headers)
    context.AddMetadata(nv.name, nv.value->template_str);
}

void
DestWorker::prepare_context_dynamic(::grpc::ClientContext &context, LogMessage *msg)
{
  g_assert(this->owner.dynamic_headers_enabled);

  LogTemplateEvalOptions options = {&this->owner.template_options, LTZ_SEND, this->super->super.seq_num, NULL,
                                    LM_VT_STRING
                                   };

  ScratchBuffersMarker marker;
  GString *buf = scratch_buffers_alloc_and_mark(&marker);

  for (auto nv : owner.headers)
    {
      if (log_template_is_literal_string(nv.value))
        {
          context.AddMetadata(nv.name, log_template_get_literal_value(nv.value, NULL));
          continue;
        }

      log_template_format(nv.value, msg, &options, buf);
      context.AddMetadata(nv.name, buf->str);
    }

  scratch_buffers_reclaim_marked(marker);
}

/* C Wrappers */

static gboolean
_init(LogThreadedDestWorker *s)
{
  GrpcDestWorker *self = (GrpcDestWorker *) s;
  return self->cpp->init();
}

static void
_deinit(LogThreadedDestWorker *s)
{
  GrpcDestWorker *self = (GrpcDestWorker *) s;
  self->cpp->deinit();
}

static gboolean
_connect(LogThreadedDestWorker *s)
{
  GrpcDestWorker *self = (GrpcDestWorker *) s;
  return self->cpp->connect();
}

static void
_disconnect(LogThreadedDestWorker *s)
{
  GrpcDestWorker *self = (GrpcDestWorker *) s;
  self->cpp->disconnect();
}

LogThreadedResult
_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  GrpcDestWorker *self = (GrpcDestWorker *) s;
  return self->cpp->insert(msg);
}

LogThreadedResult
_flush(LogThreadedDestWorker *s, LogThreadedFlushMode mode)
{
  GrpcDestWorker *self = (GrpcDestWorker *) s;
  return self->cpp->flush(mode);
}

static void
_free(LogThreadedDestWorker *s)
{
  GrpcDestWorker *self = (GrpcDestWorker *) s;
  delete self->cpp;
  log_threaded_dest_worker_free_method(s);
}

GrpcDestWorker *
grpc_dw_new(GrpcDestDriver *o, gint worker_index)
{
  GrpcDestWorker *self = g_new0(GrpcDestWorker, 1);

  log_threaded_dest_worker_init_instance(&self->super, &o->super, worker_index);
  self->super.init = _init;
  self->super.deinit = _deinit;
  self->super.connect = _connect;
  self->super.disconnect = _disconnect;
  self->super.insert = _insert;
  self->super.flush = _flush;
  self->super.free_fn = _free;

  return self;
}
