/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balazs Scheidler <bazsi@balabit.hu>
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
#ifndef LOGPROTO_MULTILINE_SERVER_INCLUDED
#define LOGPROTO_MULTILINE_SERVER_INCLUDED

#include "logproto/logproto-regexp-multiline-server.h"

enum
{
  MLM_NONE,
  MLM_INDENTED,
  MLM_PREFIX_GARBAGE,
  MLM_PREFIX_SUFFIX,
};

typedef struct _LogProtoMultiLineServerOptions
{
  LogProtoServerOptions super;
  gint mode;
  MultiLineRegexp *prefix;
  MultiLineRegexp *garbage;
} LogProtoMultiLineServerOptions;

LogProtoServer *
log_proto_multiline_server_new(LogTransport *transport,
                               const LogProtoMultiLineServerOptions *options);

gboolean log_proto_multi_line_server_options_set_mode(LogProtoMultiLineServerOptions *options, const gchar *mode);
gboolean log_proto_multi_line_server_options_set_prefix(LogProtoMultiLineServerOptions *options,
                                                        const gchar *prefix_regexp, GError **error);
gboolean log_proto_multi_line_server_options_set_garbage(LogProtoMultiLineServerOptions *options,
                                                         const gchar *garbage_regexp, GError **error);

void log_proto_multi_line_server_options_defaults(LogProtoMultiLineServerOptions *options);
void log_proto_multi_line_server_options_init(LogProtoMultiLineServerOptions *options);
void log_proto_multi_line_server_options_destroy(LogProtoMultiLineServerOptions *options);


#endif
