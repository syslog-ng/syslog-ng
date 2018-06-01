/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef LOGPROTO_CLIENT_H_INCLUDED
#define LOGPROTO_CLIENT_H_INCLUDED

#include "logproto.h"
#include "persist-state.h"

typedef struct _LogProtoClient LogProtoClient;

#define LOG_PROTO_CLIENT_OPTIONS_SIZE 128

typedef struct _LogProtoClientOptions
{
} LogProtoClientOptions;

typedef union _LogProtoClientOptionsStorage
{
  LogProtoClientOptions super;
  gchar __padding[LOG_PROTO_CLIENT_OPTIONS_SIZE];
} LogProtoClientOptionsStorage;

typedef void (*LogProtoClientAckCallback)(gint num_msg_acked, gpointer user_data);
typedef void (*LogProtoClientRewindCallback)(gpointer user_data);

typedef struct
{
  LogProtoClientAckCallback ack_callback;
  LogProtoClientRewindCallback rewind_callback;
  gpointer user_data;
} LogProtoClientFlowControlFuncs;

void log_proto_client_options_defaults(LogProtoClientOptions *options);
void log_proto_client_options_init(LogProtoClientOptions *options, GlobalConfig *cfg);
void log_proto_client_options_destroy(LogProtoClientOptions *options);

struct _LogProtoClient
{
  LogProtoStatus status;
  const LogProtoClientOptions *options;
  LogTransport *transport;
  /* FIXME: rename to something else */
  gboolean (*prepare)(LogProtoClient *s, gint *fd, GIOCondition *cond, gint *timeout);
  LogProtoStatus (*post)(LogProtoClient *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed);
  LogProtoStatus (*flush)(LogProtoClient *s);
  gboolean (*validate_options)(LogProtoClient *s);
  gboolean (*handshake_in_progess)(LogProtoClient *s);
  LogProtoStatus (*handshake)(LogProtoClient *s);
  gboolean (*restart_with_state)(LogProtoClient *s, PersistState *state, const gchar *persist_name);
  void (*free_fn)(LogProtoClient *s);
  LogProtoClientFlowControlFuncs flow_control_funcs;
};

static inline void
log_proto_client_set_client_flow_control(LogProtoClient *self, LogProtoClientFlowControlFuncs *flow_control_funcs)
{
  self->flow_control_funcs.ack_callback = flow_control_funcs->ack_callback;
  self->flow_control_funcs.rewind_callback = flow_control_funcs->rewind_callback;
  self->flow_control_funcs.user_data = flow_control_funcs->user_data;
}
static inline void
log_proto_client_msg_ack(LogProtoClient *self, gint num_msg_acked)
{
  if (self->flow_control_funcs.ack_callback)
    self->flow_control_funcs.ack_callback(num_msg_acked, self->flow_control_funcs.user_data);
}

static inline void
log_proto_client_msg_rewind(LogProtoClient *self)
{
  if (self->flow_control_funcs.rewind_callback)
    self->flow_control_funcs.rewind_callback(self->flow_control_funcs.user_data);
}

static inline gboolean
log_proto_client_validate_options(LogProtoClient *self)
{
  return self->validate_options(self);
}

static inline gboolean
log_proto_client_handshake_in_progress(LogProtoClient *s)
{
  if (s->handshake_in_progess)
    {
      return s->handshake_in_progess(s);
    }
  return FALSE;
}

static inline LogProtoStatus
log_proto_client_handshake(LogProtoClient *s)
{
  if (s->handshake)
    {
      return s->handshake(s);
    }
  return LPS_SUCCESS;
}

static inline gboolean
log_proto_client_prepare(LogProtoClient *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  return s->prepare(s, fd, cond, timeout);
}

static inline LogProtoStatus
log_proto_client_flush(LogProtoClient *s)
{
  if (s->flush)
    return s->flush(s);
  else
    return LPS_SUCCESS;
}

static inline LogProtoStatus
log_proto_client_post(LogProtoClient *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed)
{
  return s->post(s, logmsg, msg, msg_len, consumed);
}

static inline gint
log_proto_client_get_fd(LogProtoClient *s)
{
  /* FIXME: Layering violation */
  return s->transport->fd;
}

static inline void
log_proto_client_reset_error(LogProtoClient *s)
{
  s->status = LPS_SUCCESS;
}

static inline gboolean
log_proto_client_restart_with_state(LogProtoClient *s, PersistState *state, const gchar *persist_name)
{
  if (s->restart_with_state)
    return s->restart_with_state(s, state, persist_name);
  return FALSE;
}

gboolean log_proto_client_validate_options(LogProtoClient *self);
void log_proto_client_init(LogProtoClient *s, LogTransport *transport, const LogProtoClientOptions *options);
void log_proto_client_free(LogProtoClient *s);
void log_proto_client_free_method(LogProtoClient *s);

#define DEFINE_LOG_PROTO_CLIENT(prefix) \
  static gpointer                                                       \
  prefix ## _client_plugin_construct(Plugin *self)            \
  {                                                                     \
    static LogProtoClientFactory proto = {                              \
      .construct = prefix ## _client_new,                               \
    };                                                                  \
    return &proto;                                                      \
  }

#define LOG_PROTO_CLIENT_PLUGIN(prefix, __name) \
  {             \
    .type = LL_CONTEXT_CLIENT_PROTO,            \
    .name = __name,         \
    .construct = prefix ## _client_plugin_construct,  \
  }

#define LOG_PROTO_CLIENT_PLUGIN_WITH_GRAMMAR(__parser, __name) \
  {             \
    .type = LL_CONTEXT_CLIENT_PROTO,            \
    .name = __name,         \
    .parser = &__parser,  \
  }

typedef struct _LogProtoClientFactory LogProtoClientFactory;

struct _LogProtoClientFactory
{
  LogProtoClient *(*construct)(LogTransport *transport, const LogProtoClientOptions *options);
  gint default_inet_port;
  gboolean use_multitransport;
};

static inline LogProtoClient *
log_proto_client_factory_construct(LogProtoClientFactory *self, LogTransport *transport,
                                   const LogProtoClientOptions *options)
{
  return self->construct(transport, options);
}

LogProtoClientFactory *log_proto_client_get_factory(PluginContext *context, const gchar *name);

#endif
