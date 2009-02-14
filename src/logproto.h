#ifndef LOGPROTO_H_INCLUDED
#define LOGPROTO_H_INCLUDED

#include "logtransport.h"
#include "serialize.h"

typedef struct _LogProto LogProto;
typedef struct _LogProtoPlainServer LogProtoPlainServer;

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
  guint flags;
  gboolean (*read_state)(LogProto *s, SerializeArchive *archive);
  gboolean (*write_state)(LogProto *s, SerializeArchive *archive);
  void (*reset_state)(LogProto *s);
  gboolean (*prepare)(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout);
  LogProtoStatus (*fetch)(LogProto *s, const guchar **msg, gsize *msg_len, GSockAddr **sa, gboolean *may_read);
  LogProtoStatus (*post)(LogProto *s, guchar *msg, gsize msg_len, gboolean *consumed);
  void (*free_fn)(LogProto *s);
};

static inline gboolean
log_proto_read_state(LogProto *s, SerializeArchive *archive)
{
  if (s->read_state)
    return s->read_state(s, archive);
  return TRUE;
}

static inline gboolean
log_proto_write_state(LogProto *s, SerializeArchive *archive)
{
  if (s->write_state)
    return s->write_state(s, archive);
  return TRUE;
}

static inline void
log_proto_reset_state(LogProto *s)
{
  if (s->reset_state)
    s->reset_state(s);
}

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

/* */
LogProto *log_proto_plain_new_server(LogTransport *transport, gint padding_size, gint max_msg_size, guint flags);
LogProto *log_proto_plain_new_client(LogTransport *transport);
gboolean log_proto_plain_server_is_preemptable(LogProtoPlainServer *s);

/* framed */
LogProto *log_proto_framed_new_client(LogTransport *transport);
LogProto *log_proto_framed_new_server(LogTransport *transport, gint max_msg_size);

#endif
