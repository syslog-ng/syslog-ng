#ifndef LOGPROTO_PROC_KMSG_READER_H_INCLUDED
#define LOGPROTO_PROC_KMSG_READER_H_INCLUDED

#include "logproto-text-server.h"

static inline LogProtoServer *
log_proto_linux_proc_kmsg_reader_new(LogTransport *transport, const LogProtoServerOptions *options)
{
  LogProtoServer *proto;

  proto = log_proto_text_server_new(transport, options);
  ((LogProtoTextServer *) proto)->super.no_multi_read = TRUE;
  return proto;
}

#endif
