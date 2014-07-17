/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#ifndef LOGPROTO_H_INCLUDED
#define LOGPROTO_H_INCLUDED

#include "logtransport.h"
#include "persist-state.h"
#include "state.h"
#include "tlscontext.h"
#include "syslog-ng.h"
#include "bookmark.h"

#include <pcre.h>

#define FILE_CURPOS_PREFIX "affile_sd_curpos"

#define LOG_PROTO_OPTIONS_SIZE 256

/* flags for log proto plain server */
/* end-of-packet terminates log message (UDP sources) */

/* issue a single read in a poll loop as /proc/kmsg does not support non-blocking mode */
#define LPBS_NOMREAD        0x0001
/* don't exit when EOF is read */
#define LPBS_IGNORE_EOF     0x0002
/* track the current file position */
#define LPBS_POS_TRACKING   0x0004

#define LPRS_BINARY         0x0008

#define LPBS_KEEP_ONE       0x0010

#define LPBS_FSYNC          0x0020

#define LPBS_EXTERNAL_ACK_REQUIRED   0x0040


typedef struct _LogProtoFactory LogProtoFactory;

typedef struct _LogProtoOptionsBase
{
  guint flags;
  gint  size;
} LogProtoOptionsBase;

typedef struct _LogProtoOptions
{
  LogProtoOptionsBase super;
  char __padding[LOG_PROTO_OPTIONS_SIZE];
} LogProtoOptions;

typedef struct _LogProtoServerOptions
{
  LogProtoOptionsBase super;
  union
  {
    struct
    {
      pcre *prefix_matcher;
      pcre *garbage_matcher;
      gchar *prefix_pattern;
      gchar *garbage_pattern;
    } opts;
    char __padding[LOG_PROTO_OPTIONS_SIZE];
  };
} LogProtoServerOptions;

typedef enum
{
  LPS_SUCCESS,
  LPS_ERROR,
  LPS_EOF,
  LPS_AGAIN,
} LogProtoStatus;

typedef enum
{
  LPT_SERVER,
  LPT_CLIENT,
} LogProtoType;

typedef void (*LogProtoTimeoutCallback)(gpointer user_data);

typedef void (*LogProtoClientAckCallback)(guint num_msg_acked, gpointer user_data);
typedef void (*LogProtoClientRewindCallback)(gpointer user_data);

typedef struct
{
  LogProtoClientAckCallback ack_callback;
  LogProtoClientRewindCallback rewind_callback;
  gpointer user_data;
} LogProtoFlowControlFuncs;

typedef struct _LogProto LogProto;

struct _LogProto
{
  LogTransport *transport;
  GIConv convert;
  gchar *encoding;
  guint16 flags;
  gboolean keep_one_message;
  guint pending_ack_count;
  gboolean position_tracked;

  /* FIXME: rename to something else */
  gboolean (*prepare)(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout);
  gboolean (*is_preemptable)(LogProto *s);
  gboolean (*restart_with_state)(LogProto *s, PersistState *state, const gchar *persist_name);
  LogProtoStatus (*fetch)(LogProto *s, const guchar **msg, gsize *msg_len, GSockAddr **sa, gboolean *may_read, gboolean flush, Bookmark *bookmark);
  LogProtoStatus (*post)(LogProto *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed);
  LogProtoStatus (*flush)(LogProto *s);
  gboolean (*handshake_in_progess)(LogProto *s);
  LogProtoStatus (*handshake)(LogProto *s);
  void (*free_fn)(LogProto *s);
  void (*reset_state)(LogProto *s);
  /* This function is available only the object is _LogProtoTextServer */
  guint64 (*get_pending_pos)(LogProto *s, Bookmark *bookmark);
  void (*apply_state)(LogProto *s, StateHandler *state_handler);
  gboolean is_multi_line;
  LogProtoOptions *options;

  LogProtoFlowControlFuncs flow_control_funcs;
};

static inline void
log_proto_set_flow_control_funcs(LogProto *self, LogProtoFlowControlFuncs *flow_control_funcs)
{
  self->flow_control_funcs.ack_callback = flow_control_funcs->ack_callback;
  self->flow_control_funcs.rewind_callback = flow_control_funcs->rewind_callback;
  self->flow_control_funcs.user_data = flow_control_funcs->user_data;
}

static inline void
log_proto_msg_ack(LogProto *self, guint num_msg_acked)
{
  if (self->flow_control_funcs.ack_callback)
    self->flow_control_funcs.ack_callback(num_msg_acked, self->flow_control_funcs.user_data);
}

static inline void
log_proto_msg_rewind(LogProto *self)
{
  if (self->flow_control_funcs.rewind_callback)
    self->flow_control_funcs.rewind_callback(self->flow_control_funcs.user_data);
}

static inline gboolean
log_proto_handshake_in_progress(LogProto *s)
{
  if (s->handshake_in_progess)
    {
      return s->handshake_in_progess(s);
    }
  return FALSE;
}

static inline LogProtoStatus
log_proto_handshake(LogProto *s)
{
  if (s->handshake)
    {
      return s->handshake(s);
    }
  return LPS_SUCCESS;
}

static inline void
log_proto_apply_state(LogProto *s, StateHandler *state_handler)
{
  if (s->apply_state)
    {
      s->apply_state(s, state_handler);
    }
}

static inline void
log_proto_set_options(LogProto *s,LogProtoOptions *options)
{
  if (options)
    {
      s->options = options;
    }
}

static inline gboolean
log_proto_is_position_tracked(LogProto *s)
{
  return s->position_tracked;
}

static inline gboolean
log_proto_prepare(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  return s->prepare(s, fd, cond, timeout);
}

static inline gboolean
log_proto_is_preemptable(LogProto *s)
{
  if (s->is_preemptable)
    return s->is_preemptable(s);
  return TRUE;
}

static inline gboolean
log_proto_restart_with_state(LogProto *s, PersistState *state, const gchar *persist_name)
{
  if (s->restart_with_state)
    return s->restart_with_state(s, state, persist_name);
  return FALSE;
}

static inline LogProtoStatus
log_proto_flush(LogProto *s)
{
  if (s->flush)
    return s->flush(s);
  else
    return LPS_SUCCESS;
}

static inline LogProtoStatus
log_proto_post(LogProto *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed)
{
  return s->post(s, logmsg, msg, msg_len, consumed);
}

static inline LogProtoStatus
log_proto_fetch(LogProto *s, const guchar **msg, gsize *msg_len, GSockAddr **sa, gboolean *may_read, gboolean flush, Bookmark *bookmark)
{
  return s->fetch(s, msg, msg_len, sa, may_read, flush, bookmark);
}

static inline void
log_proto_reset_state(LogProto *s)
{
  if(s->reset_state)
    s->reset_state(s);
}

static inline gint
log_proto_get_fd(LogProto *s)
{
  /* FIXME: Layering violation */
  return s->transport->fd;
}

static inline void
log_proto_set_keep_one_message(LogProto *self, gboolean value)
{
  self->keep_one_message = value;
}

gboolean log_proto_set_encoding(LogProto *s, const gchar *encoding);
void log_proto_free(LogProto *s);

struct _LogProtoFactory
{
  LogProto *(*create)(LogTransport *transport, LogProtoOptions *options, GlobalConfig *cfg);
  LogTransport *(*construct_transport)(LogProtoOptions *options, int fd, int flags, TLSContext *tls_context);
  guint default_port;
};

LogProtoFactory *log_proto_get_factory(GlobalConfig *cfg,LogProtoType type,const gchar *name);
void log_proto_check_server_options(LogProtoServerOptions *options);

#endif
