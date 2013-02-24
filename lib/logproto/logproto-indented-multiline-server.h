/*
 * Copyright (c) 2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2012 Gergely Nagy <algernon@balabit.hu>
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
 */
#ifndef LOGPROTO_INDENTED_MULTILINE_SERVER_INCLUDED
#define LOGPROTO_INDENTED_MULTILINE_SERVER_INCLUDED

#include "logproto-text-server.h"

typedef struct _LogProtoIMultiLineServer LogProtoIMultiLineServer;
struct _LogProtoIMultiLineServer
{
  LogProtoTextServer super;
};

/* LogProtoIMultiLineServer
 *
 * This class processes indented multiline text files/streams.  Each
 * record consists of one line that starts with non-whitespace, with
 * zero or more lines starting with whitespace. A record is terminated
 * when we reach a line that starts with non-whitespace, or EOF.
 */
LogProtoServer *log_proto_indented_multiline_server_new(LogTransport *transport,
                                                        const LogProtoServerOptions *options);
void log_proto_indented_multiline_server_init(LogProtoIMultiLineServer *self,
                                              LogTransport *transport,
                                              const LogProtoServerOptions *options);

#endif
