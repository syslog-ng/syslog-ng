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
typedef struct _LogProtoTextServer LogProtoTextServer;
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
  gboolean (*is_preemptable)(LogProto *s);
  gboolean (*restart_with_state)(LogProto *s, PersistState *state, const gchar *persist_name);
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

/* issue a single read in a poll loop as /proc/kmsg does not support non-blocking mode */
#define LPBS_NOMREAD        0x0001
/* don't exit when EOF is read */
#define LPBS_IGNORE_EOF     0x0002
/* track the current file position */
#define LPBS_POS_TRACKING   0x0004

#define LPRS_BINARY         0x0008

/*
 * LogProtoRecordServer
 *
 * This class reads input in equally sized chunks. If LPRS_BINARY is
 * specified the record is returned as a whole, if it is not, then the
 * message lasts until the first EOL/NUL character.
 */
LogProto *log_proto_record_server_new(LogTransport *transport, gint record_size, guint flags);

/*
 * LogProtoDGramServer
 *
 * This class reads input as datagrams, each datagram is a separate
 * message, regardless of embedded EOL/NUL characters.
 */
LogProto *log_proto_dgram_server_new(LogTransport *transport, gint max_msg_size, guint flags);

/* LogProtoTextServer
 *
 * This class processes text files/streams. Each record is terminated via an EOL character.
 */
LogProto *log_proto_text_server_new(LogTransport *transport, gint max_msg_size, guint flags);

/*
 * LogProtoTextClient
 */
LogProto *log_proto_text_client_new(LogTransport *transport);

/* framed */
LogProto *log_proto_framed_client_new(LogTransport *transport);
LogProto *log_proto_framed_server_new(LogTransport *transport, gint max_msg_size);

#endif
