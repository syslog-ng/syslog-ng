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
  gboolean drop_input;
  gint idle_timeout;
} LogProtoClientOptions;

typedef union _LogProtoClientOptionsStorage
{
  LogProtoClientOptions super;
  gchar __padding[LOG_PROTO_CLIENT_OPTIONS_SIZE];
} LogProtoClientOptionsStorage;
// _Static_assert() is a C11 feature, so we use a typedef trick to perform the static assertion
typedef char static_assert_size_check_LogProtoClientOptions[
   LOG_PROTO_CLIENT_OPTIONS_SIZE >= sizeof(LogProtoClientOptions) ? 1 : -1];

typedef void (*LogProtoClientAckCallback)(gint num_msg_acked, gpointer user_data);
typedef void (*LogProtoClientRewindCallback)(gpointer user_data);

typedef struct
{
  LogProtoClientAckCallback ack_callback;
  LogProtoClientRewindCallback rewind_callback;
  gpointer user_data;
} LogProtoClientFlowControlFuncs;

void log_proto_client_options_set_drop_input(LogProtoClientOptionsStorage *options, gboolean drop_input);
void log_proto_client_options_set_timeout(LogProtoClientOptionsStorage *options, gint timeout);
gint log_proto_client_options_get_timeout(LogProtoClientOptionsStorage *options);

void log_proto_client_options_defaults(LogProtoClientOptionsStorage *options);
void log_proto_client_options_init(LogProtoClientOptionsStorage *options, GlobalConfig *cfg);
void log_proto_client_options_destroy(LogProtoClientOptionsStorage *options);

struct _LogProtoClient
{
  LogProtoStatus status;
  const LogProtoClientOptionsStorage *options;
  LogTransportStack transport_stack;
  gboolean (*poll_prepare)(LogProtoClient *s, GIOCondition *cond, GIOCondition *idle_cond, gint *timeout);
  LogProtoStatus (*post)(LogProtoClient *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed);
  LogProtoStatus (*process_in)(LogProtoClient *s);
  LogProtoStatus (*flush)(LogProtoClient *s);
  gboolean (*validate_options)(LogProtoClient *s);
  LogProtoStatus (*handshake)(LogProtoClient *s, gboolean *handshake_finished);
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

static inline void
log_proto_client_set_options(LogProtoClient *self, const LogProtoClientOptionsStorage *options)
{
  self->options = options;
}

static inline gboolean
log_proto_client_validate_options(LogProtoClient *self)
{
  return self->validate_options(self);
}

static inline LogProtoStatus
log_proto_client_handshake(LogProtoClient *s, gboolean *handshake_finished)
{
  if (s->handshake)
    {
      return s->handshake(s, handshake_finished);
    }
  *handshake_finished = TRUE;
  return LPS_SUCCESS;
}

static inline gboolean
log_proto_client_poll_prepare(LogProtoClient *self, GIOCondition *cond, GIOCondition *idle_cond, gint *timeout)
{
  GIOCondition transport_cond = 0;
  gboolean result = TRUE;

  if (log_transport_stack_poll_prepare(&self->transport_stack, &transport_cond))
    goto exit;

  result = self->poll_prepare(self, cond, idle_cond, timeout);

  if (!result && *timeout < 0)
    *timeout = self->options->super.idle_timeout;

exit:
  /* transport I/O needs take precedence */
  if (transport_cond != 0)
    *cond = transport_cond;

  return result;
}

static inline LogProtoStatus log_proto_client_process_in(LogProtoClient *s);

static inline LogProtoStatus
log_proto_client_flush(LogProtoClient *self)
{
  if (log_transport_stack_get_io_requirement(&self->transport_stack) == LTIO_READ_WANTS_WRITE)
    return self->process_in(self);

  return self->flush(self);
}

static inline LogProtoStatus
log_proto_client_process_in(LogProtoClient *self)
{
  if (log_transport_stack_get_io_requirement(&self->transport_stack) == LTIO_WRITE_WANTS_READ)
    return self->flush(self);

  return self->process_in(self);
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
  return s->transport_stack.fd;
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
void log_proto_client_init(LogProtoClient *s, LogTransport *transport, const LogProtoClientOptionsStorage *options);
void log_proto_client_free(LogProtoClient *s);
void log_proto_client_free_method(LogProtoClient *s);

#define DEFINE_LOG_PROTO_CLIENT(prefix, options...) \
  static gpointer                                                       \
  prefix ## _client_plugin_construct(Plugin *self)            \
  {                                                                     \
    static LogProtoClientFactory proto = {                              \
      .construct = prefix ## _client_new,                               \
      .stateful  = FALSE,                                               \
      ##options \
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
  LogProtoClient *(*construct)(LogTransport *transport, const LogProtoClientOptionsStorage *options);
  gint default_inet_port;
  gboolean stateful;
};

static inline LogProtoClient *
log_proto_client_factory_construct(LogProtoClientFactory *self, LogTransport *transport,
                                   const LogProtoClientOptionsStorage *options)
{
  return self->construct(transport, options);
}

static inline gboolean
log_proto_client_factory_is_proto_stateful(LogProtoClientFactory *self)
{
  return self->stateful;
}

LogProtoClientFactory *log_proto_client_get_factory(PluginContext *context, const gchar *name);

#endif
