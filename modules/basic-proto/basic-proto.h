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

#ifndef BASEPROTO_H_INCLUDED
#define BASEPROTO_H_INCLUDED

#include "logtransport.h"
#include "logproto.h"

/* flags for log proto plain server */
/* end-of-packet terminates log message (UDP sources) */

/* issue a single read in a poll loop as /proc/kmsg does not support non-blocking mode */
#define LPBS_NOMREAD        0x0001
/* don't exit when EOF is read */
#define LPBS_IGNORE_EOF     0x0002
/* track the current file position */
#define LPBS_POS_TRACKING   0x0004

#define LPRS_BINARY         0x0008

LogProto *log_proto_framed_client_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg);
LogProto *log_proto_text_client_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg);
LogProto *log_proto_record_client_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg);
LogProto *log_proto_dgram_client_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg);
LogProto *log_proto_file_writer_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg);

LogProto *log_proto_framed_server_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg);
LogProto *log_proto_text_server_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg);
LogProto *log_proto_record_server_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg);
LogProto *log_proto_dgram_server_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg);
LogProto *log_proto_file_reader_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg);

#endif
