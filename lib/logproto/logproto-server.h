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

#ifndef LOGPROTO_SERVER_H_INCLUDED
#define LOGPROTO_SERVER_H_INCLUDED

#include "logproto.h"
#include "persist-state.h"
#include "transport/transport-aux-data.h"
#include "ack-tracker/bookmark.h"
#include "multi-line/multi-line-factory.h"

typedef struct _LogProtoServer LogProtoServer;
typedef struct _LogProtoServerOptions LogProtoServerOptions;
typedef union _LogProtoServerOptionsStorage LogProtoServerOptionsStorage;

typedef enum
{
  LPPA_POLL_IO,
  LPPA_FORCE_SCHEDULE_FETCH,
  LPPA_SUSPEND
} LogProtoPrepareAction;

#define LOG_PROTO_SERVER_OPTIONS_SIZE 160

struct _LogProtoServerOptions
{
  void (*init)(LogProtoServerOptionsStorage *self, GlobalConfig *cfg);
  void (*destroy)(LogProtoServerOptionsStorage *self);
  gboolean (*validate)(LogProtoServerOptionsStorage *self);
  gboolean initialized;

  gchar *encoding;
  /* maximum message length in bytes */
  gint max_msg_size;
  gboolean trim_large_messages;
  gint init_buffer_size;
  gint max_buffer_size;
  gint idle_timeout;
  AckTrackerFactory *ack_tracker_factory;
  MultiLineOptions multi_line_options;
};

/* See logproto-file-reader.h and logreader.c - log_reader_options_defaults for the details of the options mess */
union _LogProtoServerOptionsStorage
{
  LogProtoServerOptions super;
  gchar __padding[LOG_PROTO_SERVER_OPTIONS_SIZE];
};
// _Static_assert() is a C11 feature, so we use a typedef trick to perform the static assertion
typedef char static_assert_size_check_LogProtoServerOptions[
   LOG_PROTO_SERVER_OPTIONS_SIZE >= sizeof(LogProtoServerOptions) ? 1 : -1];

gboolean log_proto_server_options_set_encoding(LogProtoServerOptionsStorage *s, const gchar *encoding);
void log_proto_server_options_set_ack_tracker_factory(LogProtoServerOptionsStorage *s, AckTrackerFactory *factory);
void log_proto_server_options_defaults(LogProtoServerOptionsStorage *options);
void log_proto_server_options_init(LogProtoServerOptionsStorage *options, GlobalConfig *cfg);
gboolean log_proto_server_options_validate(LogProtoServerOptionsStorage *options);
void log_proto_server_options_destroy(LogProtoServerOptionsStorage *options);

typedef void (*LogProtoServerWakeupFunc)(gpointer user_data);
typedef struct _LogProtoServerWakeupCallback
{
  LogProtoServerWakeupFunc func;
  gpointer user_data;
} LogProtoServerWakeupCallback;

struct _LogProtoServer
{
  LogProtoStatus status;
  const LogProtoServerOptionsStorage *options;
  LogTransportStack transport_stack;
  AckTracker *ack_tracker;

  LogProtoServerWakeupCallback wakeup_callback;
  LogProtoPrepareAction (*poll_prepare)(LogProtoServer *s, GIOCondition *cond, gint *timeout);
  gboolean (*restart_with_state)(LogProtoServer *s, PersistState *state, const gchar *persist_name);
  LogProtoStatus (*fetch)(LogProtoServer *s, const guchar **msg, gsize *msg_len, gboolean *may_read,
                          LogTransportAuxData *aux, Bookmark *bookmark);
  gboolean (*validate_options)(LogProtoServer *s);
  LogProtoStatus (*handshake)(LogProtoServer *s, gboolean *handshake_finished, LogProtoServer **proto_replacement);
  void (*free_fn)(LogProtoServer *s);
};

static inline gboolean
log_proto_server_validate_options(LogProtoServer *self)
{
  return self->validate_options(self);
}

static inline LogProtoStatus
log_proto_server_handshake(LogProtoServer *s, gboolean *handshake_finished, LogProtoServer **proto_replacement)
{
  if (s->handshake)
    {
      LogProtoStatus status;

      g_assert(*proto_replacement == NULL);
      status = s->handshake(s, handshake_finished, proto_replacement);
      if (*proto_replacement)
        {
          g_assert(status == LPS_SUCCESS || status == LPS_AGAIN);
        }
      return status;
    }
  *handshake_finished = TRUE;
  return LPS_SUCCESS;
}

static inline void
log_proto_server_set_options(LogProtoServer *self, const LogProtoServerOptionsStorage *options)
{
  self->options = options;
}

static inline const LogProtoServerOptionsStorage *
log_proto_server_get_options(LogProtoServer *self)
{
  return self->options;
}

static inline const MultiLineOptions *
log_proto_server_get_multi_line_options(LogProtoServer *self)
{
  return &self->options->super.multi_line_options;
}

static inline LogProtoPrepareAction
log_proto_server_poll_prepare(LogProtoServer *s, GIOCondition *cond, gint *timeout)
{
  LogProtoPrepareAction result = s->poll_prepare(s, cond, timeout);

  if (result == LPPA_POLL_IO && *timeout < 0)
    *timeout = s->options->super.idle_timeout;
  return result;
}

static inline gboolean
log_proto_server_restart_with_state(LogProtoServer *s, PersistState *state, const gchar *persist_name)
{
  if (s->restart_with_state)
    return s->restart_with_state(s, state, persist_name);
  return FALSE;
}

static inline LogProtoStatus
log_proto_server_fetch(LogProtoServer *s, const guchar **msg, gsize *msg_len, gboolean *may_read,
                       LogTransportAuxData *aux, Bookmark *bookmark)
{
  if (s->status == LPS_SUCCESS)
    return s->fetch(s, msg, msg_len, may_read, aux, bookmark);
  return s->status;
}

static inline gint
log_proto_server_get_fd(LogProtoServer *s)
{
  /* FIXME: Layering violation, as transport may not be fd based at all.
   * But LogReader assumes it is.  */

  return s->transport_stack.fd;
}

static inline void
log_proto_server_reset_error(LogProtoServer *s)
{
  s->status = LPS_SUCCESS;
}

static inline void
log_proto_server_set_wakeup_cb(LogProtoServer *s, LogProtoServerWakeupFunc wakeup, gpointer user_data)
{
  s->wakeup_callback.user_data = user_data;
  s->wakeup_callback.func = wakeup;
}

static inline void
log_proto_server_wakeup_cb_call(LogProtoServerWakeupCallback *wakeup_callback)
{
  if (wakeup_callback->func)
    wakeup_callback->func(wakeup_callback->user_data);
}

AckTrackerFactory *log_proto_server_get_ack_tracker_factory(LogProtoServer *s);
gboolean log_proto_server_is_position_tracked(LogProtoServer *s);

gboolean log_proto_server_validate_options_method(LogProtoServer *s);
void log_proto_server_init(LogProtoServer *s, LogTransport *transport, const LogProtoServerOptionsStorage *options);
void log_proto_server_free_method(LogProtoServer *s);
void log_proto_server_free(LogProtoServer *s);

static inline void
log_proto_server_set_ack_tracker(LogProtoServer *s, AckTracker *ack_tracker)
{
  g_assert(ack_tracker);
  s->ack_tracker = ack_tracker;
}

#define DEFINE_LOG_PROTO_SERVER(prefix, options...) \
  static gpointer                                                       \
  prefix ## _server_plugin_construct(Plugin *self)                      \
  {                                                                     \
    static LogProtoServerFactory proto = {                              \
      .construct = prefix ## _server_new,                       \
      ##options \
    };                                                                  \
    return &proto;                                                      \
  }

#define LOG_PROTO_SERVER_PLUGIN(prefix, __name) \
  {             \
    .type = LL_CONTEXT_SERVER_PROTO,            \
    .name = __name,         \
    .construct = prefix ## _server_plugin_construct,  \
  }

#define LOG_PROTO_SERVER_PLUGIN_WITH_GRAMMAR(__parser, __name) \
  {             \
    .type = LL_CONTEXT_SERVER_PROTO,            \
    .name = __name,         \
    .parser = &__parser,  \
  }

typedef struct _LogProtoServerFactory LogProtoServerFactory;

struct _LogProtoServerFactory
{
  LogProtoServer *(*construct)(LogTransport *transport, const LogProtoServerOptionsStorage *options);
  gint default_inet_port;
};

static inline LogProtoServer *
log_proto_server_factory_construct(LogProtoServerFactory *self, LogTransport *transport,
                                   const LogProtoServerOptionsStorage *options)
{
  return self->construct(transport, options);
}

LogProtoServerFactory *log_proto_server_get_factory(PluginContext *context, const gchar *name);

const guchar *find_eom(const guchar *s, gsize n);

#endif
