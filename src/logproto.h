/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef LOGPROTO_H_INCLUDED
#define LOGPROTO_H_INCLUDED

#include "logtransport.h"
#include "serialize.h"
#include "persist-state.h"

typedef struct _LogProto LogProto;
typedef struct _LogProtoPlainServer LogProtoPlainServer;
typedef struct _LogProtoFileReader LogProtoFileReader;

typedef enum
{
  LPS_SUCCESS,
  LPS_ERROR,
  LPS_EOF,
} LogProtoStatus;

struct _LogProto
{
  LogTransport *transport;
  GIConv convert;
  gchar *encoding;
  guint16 flags;
  gboolean (*prepare)(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout);
  LogProtoStatus (*fetch)(LogProto *s, const guchar **msg, gsize *msg_len, GSockAddr **sa, gboolean *may_read);
  void (*queued)(LogProto *s);
  LogProtoStatus (*post)(LogProto *s, guchar *msg, gsize msg_len, gboolean *consumed);
  void (*free_fn)(LogProto *s);
};

static inline gboolean
log_proto_prepare(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  return s->prepare(s, fd, cond, timeout);
}

static inline LogProtoStatus
log_proto_post(LogProto *s, guchar *msg, gsize msg_len, gboolean *consumed)
{
  return s->post(s, msg, msg_len, consumed);
}

static inline LogProtoStatus
log_proto_fetch(LogProto *s, const guchar **msg, gsize *msg_len, GSockAddr **sa, gboolean *may_read)
{
  return s->fetch(s, msg, msg_len, sa, may_read);
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
#define LPPF_PKTTERM        0x0001
/* issue a single read in a poll loop as /proc/kmsg does not support non-blocking mode */
#define LPPF_NOMREAD        0x0002
#define LPPF_IGNORE_EOF     0x0004
#define LPPF_POS_TRACKING   0x0008

/* */
LogProto *log_proto_plain_new_server(LogTransport *transport, gint padding_size, gint max_msg_size, guint flags);
LogProto *log_proto_plain_new_client(LogTransport *transport);

/* file reader with persistent state */
gboolean log_proto_file_reader_is_preemptable(LogProtoFileReader *s);
gboolean log_proto_file_reader_restart_with_state(LogProto *s, PersistState *persist_state, const gchar *persist_name);
LogProto *log_proto_file_reader_new(LogTransport *transport, const gchar *filename, gint padding_size, gint max_msg_size, guint flags);


/* framed */
LogProto *log_proto_framed_new_client(LogTransport *transport);
LogProto *log_proto_framed_new_server(LogTransport *transport, gint max_msg_size);

#endif
