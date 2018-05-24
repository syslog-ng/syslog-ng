/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#ifndef LOGPROTO_BUFFERED_SERVER_H_INCLUDED
#define LOGPROTO_BUFFERED_SERVER_H_INCLUDED

#include "logproto-server.h"
#include "persistable-state-header.h"

enum
{
  LPBSF_FETCHING_FROM_INPUT,
  LPBSF_FETCHING_FROM_BUFFER,
};

typedef struct _LogProtoBufferedServerState
{
  /* NOTE: that if you add/remove structure members you have to update
   * the byte order swap code in LogProtoFileReader for mulit-byte
   * members. */

  PersistableStateHeader header;
  guint8 raw_buffer_leftover_size;
  guint8 __padding1[1];
  guint32 buffer_pos;
  guint32 pending_buffer_end;
  guint32 buffer_size;
  guint32 __deprecated_buffer_cached_eol;
  guint32 pending_buffer_pos;

  /* the stream position where we converted out current buffer from (offset in file) */
  gint64 raw_stream_pos;
  gint64 pending_raw_stream_pos;
  /* the size of raw data (measured in bytes) that got converted from raw_stream_pos into our buffer */
  gint32 raw_buffer_size;
  gint32 pending_raw_buffer_size;
  guchar raw_buffer_leftover[8];

  gint64 file_size;
  gint64 file_inode;
} LogProtoBufferedServerState;

typedef struct _LogProtoBufferedServer LogProtoBufferedServer;
struct _LogProtoBufferedServer
{
  LogProtoServer super;
  gboolean (*fetch_from_buffer)(LogProtoBufferedServer *self, const guchar *buffer_start, gsize buffer_bytes,
                                const guchar **msg, gsize *msg_len);
  gint (*read_data)(LogProtoBufferedServer *self, guchar *buf, gsize len, LogTransportAuxData *aux);

  gboolean
  /* track & record the position in the input, to be used for file
   * position tracking.  It's not always on as it's expensive when
   * an encoding is specified and the last record in the input is
   * not complete.
   */
  pos_tracking:1,

               /* specifies that the input is a stream of bytes, size of chunks
                * read split the input randomly.  Non-stream based stuff is udp
                * or fixed-size records read from a file.  */
               stream_based:1,

               no_multi_read:1;
  gint fetch_state;
  GIOStatus io_status;
  LogProtoBufferedServerState *state1;
  PersistState *persist_state;
  PersistEntryHandle persist_handle;
  GIConv convert;
  guchar *buffer;

  /* auxiliary data (e.g. GSockAddr, other transport related meta
   * data) associated with the already buffered data */
  LogTransportAuxData buffer_aux;
};

static inline gboolean
log_proto_buffered_server_is_input_closed(LogProtoBufferedServer *self)
{
  return self->io_status != G_IO_STATUS_NORMAL;
}

gboolean log_proto_buffered_server_prepare(LogProtoServer *s, GIOCondition *cond, gint *timeout G_GNUC_UNUSED);
LogProtoBufferedServerState *log_proto_buffered_server_get_state(LogProtoBufferedServer *self);
void log_proto_buffered_server_put_state(LogProtoBufferedServer *self);

/* LogProtoBufferedServer */
gboolean log_proto_buffered_server_validate_options_method(LogProtoServer *s);
void log_proto_buffered_server_init(LogProtoBufferedServer *self, LogTransport *transport,
                                    const LogProtoServerOptions *options);
void log_proto_buffered_server_free_method(LogProtoServer *s);

#endif
