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
#include "tlscontext.h"

#include <regex.h>

#define MAX_STATE_DATA_LENGTH 128

typedef struct _AckDataBase
{
  gboolean not_acked;
  gboolean not_sent;
  guint64 id;
  PersistEntryHandle persist_handle;
}AckDataBase;

#define LOG_PROTO_OPTIONS_SIZE 256

typedef struct _AckData
{
  AckDataBase super;
  char other_state[MAX_STATE_DATA_LENGTH];
}AckData;

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
      regex_t *prefix_matcher;
      regex_t *garbage_matcher;
    } opts;
    char __padding[LOG_PROTO_OPTIONS_SIZE];
  };
} LogProtoServerOptions;

typedef enum
{
  LPS_SUCCESS,
  LPS_ERROR,
  LPS_EOF,
} LogProtoStatus;

typedef enum
{
  LPT_SERVER,
  LPT_CLIENT,
} LogProtoType;

typedef void (*LogProtoAckMessages)(guint num_msg_acked,gpointer user_data);
typedef void (*LogProtoTimeoutCallback)(gpointer user_data);

typedef struct _LogProto LogProto;

struct _LogProto
{
  LogTransport *transport;
  GIConv convert;
  gchar *encoding;
  guint16 flags;
  /* FIXME: rename to something else */
  gboolean (*prepare)(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout);
  gboolean (*is_preemptable)(LogProto *s);
  gboolean (*restart_with_state)(LogProto *s, PersistState *state, const gchar *persist_name);
  LogProtoStatus (*fetch)(LogProto *s, const guchar **msg, gsize *msg_len, GSockAddr **sa, gboolean *may_read, gboolean flush);
  void (*queued)(LogProto *s);
  LogProtoStatus (*post)(LogProto *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed);
  LogProtoStatus (*flush)(LogProto *s);
  void (*free_fn)(LogProto *s);
  void (*reset_state)(LogProto *s);
  /* This function is available only the object is _LogProtoTextServer */
  void (*get_info)(LogProto *s, guint64 *pos);
  void (*get_state)(LogProto *s, gpointer user_data);
  gboolean (*ack)(PersistState *state, gpointer user_data, gboolean need_to_save);
  LogProtoAckMessages ack_callback;
  gboolean (*is_reliable)(LogProto *s);
  gpointer ack_user_data;
  gboolean is_multi_line;
  PersistState *state;
  LogProtoOptions *options;
};

static inline void
log_proto_set_options(LogProto *s,LogProtoOptions *options)
{
  if (options)
    {
      s->options = options;
    }
}

static inline gboolean
log_proto_is_reliable(LogProto *s)
{
  if (s->is_reliable)
    {
      return s->is_reliable(s);
    }
  return FALSE;
}

static inline void
log_proto_set_msg_acked_callback(LogProto *s, LogProtoAckMessages callback,gpointer user_data)
{
  s->ack_callback = callback;
  s->ack_user_data = user_data;
}

static inline gboolean
log_proto_prepare(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  return s->prepare(s, fd, cond, timeout);
}

static inline void
log_proto_get_state(LogProto *s, gpointer user_data)
{
  if (s->get_state)
    {
      s->get_state(s, user_data);
    }
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
log_proto_fetch(LogProto *s, const guchar **msg, gsize *msg_len, GSockAddr **sa, gboolean *may_read, gboolean flush)
{
  return s->fetch(s, msg, msg_len, sa, may_read, flush);
}

static inline void
log_proto_reset_state(LogProto *s)
{
  if(s->reset_state)
    s->reset_state(s);
}

static inline void
log_proto_queued(LogProto *s)
{
  if (s->queued)
    s->queued(s);
}

static inline gint
log_proto_get_fd(LogProto *s)
{
  /* FIXME: Layering violation */
  return s->transport->fd;
}

gboolean log_proto_set_encoding(LogProto *s, const gchar *encoding);
void log_proto_free(LogProto *s);

/* flags for log proto plain server */
/* end-of-packet terminates log message (UDP sources) */

/* issue a single read in a poll loop as /proc/kmsg does not support non-blocking mode */
#define LPBS_NOMREAD        0x0001
/* don't exit when EOF is read */
#define LPBS_IGNORE_EOF     0x0002
/* track the current file position */
#define LPBS_POS_TRACKING   0x0004

#define LPRS_BINARY         0x0008


struct _LogProtoFactory
{
  LogProto *(*create)(LogTransport *transport, LogProtoOptions *options, GlobalConfig *cfg);
  LogTransport *(*construct_transport)(LogProtoOptions *options, int fd, int flags, TLSContext *tlscontext);
  guint default_port;
};

LogProtoFactory *log_proto_get_factory(GlobalConfig *cfg,LogProtoType type,const gchar *name);

#endif
