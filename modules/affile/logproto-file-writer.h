#ifndef LOG_PROTO_FILE_WRITER_H_INCLUDED
#define LOG_PROTO_FILE_WRITER_H_INCLUDED

#include "logproto-client.h"

LogProtoClient *log_proto_file_writer_new(LogTransport *transport, const LogProtoClientOptions *options, gint flush_lines, gboolean fsync);

#endif
