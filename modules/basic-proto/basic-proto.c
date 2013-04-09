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

#include "basic-proto.h"
#include "logproto.h"
#include "messages.h"
#include "persist-state.h"
#include "compat.h"
#include "misc.h"
#include "serialize.h"
#include "state.h"
#include "logmsg.h"

#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#ifndef G_OS_WIN32
#include <sys/uio.h>
#endif
#include <limits.h>
#include <pcre.h>

typedef struct _LogProtoTextClient
{
  LogProto super;
  guchar *partial;
  gsize partial_len, partial_pos;
  guint8  acked;
} LogProtoTextClient;

#define LPFCS_FRAME_INIT    0
#define LPFCS_FRAME_SEND    1
#define LPFCS_MESSAGE_SEND  2

typedef struct _LogProtoFramedClient
{
  LogProtoTextClient super;
  gint state;
  gchar frame_hdr_buf[9];
  gint frame_hdr_len, frame_hdr_pos;
} LogProtoFramedClient;

extern guint32 g_run_id;
const guchar *find_eom(const guchar *s, gsize n);

gboolean
log_proto_set_encoding(LogProto *self, const gchar *encoding)
{
  if (self->convert != (GIConv) -1)
    {
      g_iconv_close(self->convert);
      self->convert = (GIConv) -1;
    }
  if (self->encoding)
    {
      g_free(self->encoding);
      self->encoding = NULL;
    }

  self->convert = g_iconv_open("utf-8", encoding);
  if (self->convert == (GIConv) -1)
    return FALSE;

  self->encoding = g_strdup(encoding);
  return TRUE;
}

void
log_proto_free(LogProto *s)
{
  if (s->free_fn)
    s->free_fn(s);
  if (s->convert != (GIConv) -1)
    g_iconv_close(s->convert);
  if (s->encoding)
    g_free(s->encoding);
  if (s->transport)
    log_transport_free(s->transport);
  g_free(s);
}

static gboolean
log_proto_text_client_prepare(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  LogProtoTextClient *self = (LogProtoTextClient *) s;

  *fd = self->super.transport->fd;
  *cond = self->super.transport->cond;

  /* if there's no pending I/O in the transport layer, then we want to do a write */
  if (*cond == 0)
    *cond = G_IO_OUT;
  return !!self->partial;
}

static LogProtoStatus
log_proto_text_client_flush_buffer(LogProto *s)
{
  LogProtoTextClient *self = (LogProtoTextClient *) s;
  gint rc;
  /* attempt to flush previously buffered data */
  if (self->partial)
    {
      gint len = self->partial_len - self->partial_pos;

      rc = log_transport_write(self->super.transport, &self->partial[self->partial_pos], len);
      if (rc < 0)
        {
          if (errno != EAGAIN && errno != EINTR)
            {
              msg_error("I/O error occurred while writing",
                        evt_tag_int("fd", self->super.transport->fd),
                        evt_tag_errno(EVT_TAG_OSERROR, errno),
                        evt_tag_id(MSG_IO_ERROR_WRITING),
                        NULL);
              return LPS_ERROR;
            }
          return LPS_SUCCESS;
        }
      else if (rc != len)
        {
          self->partial_pos += rc;
          return LPS_SUCCESS;
        }
      else
        {
          g_free(self->partial);
          self->partial = NULL;
          /* NOTE: we return here to give a chance to the framed protocol to send the frame header. */
          return LPS_SUCCESS;
        }
    }
  return LPS_SUCCESS;
}

static LogProtoStatus
log_proto_text_client_flush(LogProto *s)
{
  LogProtoTextClient *self = (LogProtoTextClient *) s;

  /* attempt to flush previously buffered data */
  if (self->partial)
    {
      log_proto_text_client_flush_buffer(s);
    }
  else
    {
      log_transport_write(self->super.transport, "", 0);
    }
  if (!self->partial && self->acked)
    {
      log_proto_ack_msg(&self->super,1);
      self->acked = 0;
    }
  return self->partial ? LPS_AGAIN : LPS_SUCCESS;
}

/*
 * log_proto_text_client_post:
 * @msg: formatted log message to send (this might be consumed by this function)
 * @msg_len: length of @msg
 * @consumed: pointer to a gboolean that gets set if the message was consumed by this function
 * @error: error information, if any
 *
 * This function posts a message to the log transport, performing buffering
 * of partially sent data if needed. The return value indicates whether we
 * successfully sent this message, or if it should be resent by the caller.
 **/
static LogProtoStatus
log_proto_client_post_writer(LogProto *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed, gboolean syslog_proto)
{
  LogProtoTextClient *self = (LogProtoTextClient *) s;
  gint rc;

  /* NOTE: the client does not support charset conversion for now */
  g_assert(self->super.convert == (GIConv) -1);

  *consumed = FALSE;
  rc = log_proto_text_client_flush_buffer(s);
  if (rc == LPS_ERROR)
    {
      goto write_error;
    }
  else if (self->partial)
    {
      /* NOTE: the partial buffer has not been emptied yet even with the
       * flush above, we shouldn't attempt to write again.
       *
       * Otherwise: with the framed protocol this could case the frame
       * header to be split, and interleaved with message payload, as in:
       *
       *     First bytes of frame header || payload || tail of frame header.
       *
       * This obviously would cause the framing to break. Also libssl
       * returns an error in this case, which is how this was discovered.
       */
      return rc;
    }
  else if (self->partial_len != 0 && !self->partial && syslog_proto)
    {
      LogProtoFramedClient *framed_self = (LogProtoFramedClient *) s;
      self->partial_pos =0;
      self->partial_len =0;
      framed_self->state = LPFCS_FRAME_INIT;
      return rc;
    }

  /* OK, partial buffer empty, now flush msg that we just got */

  rc = log_transport_write(self->super.transport, msg, msg_len);

  if (rc < 0 || rc != msg_len)
    {
      /* error OR partial flush, we sent _some_ of the message that we got, save it to self->partial and tell the caller that we consumed it */
      if (rc < 0 && errno != EAGAIN && errno != EINTR)
        goto write_error;

      /* NOTE: log_proto_framed_client_post depends on our current
       * behaviour, that we consume every message that we can, even if we
       * couldn't write a single byte out.
       *
       * If we return LPS_SUCCESS and self->partial == NULL, it assumes that
       * the message was sent.
       */


      self->partial = msg;
      self->partial_len = msg_len;
      self->partial_pos = rc > 0 ? rc : 0;
      *consumed = TRUE;
    }
  else
    {
      /* all data was nicely sent */
      g_free(msg);
      *consumed = TRUE;
    }
  if (self->super.flags & LPBS_KEEP_ONE)
    {
      self->acked++;
      if (self->acked > 1)
        {
          log_proto_ack_msg(&self->super,1);
          self->acked--;
        }
    }
  else
    {
      log_proto_ack_msg(&self->super,1);
    }
  return LPS_SUCCESS;

 write_error:
  if (errno != EAGAIN && errno != EINTR)
    {
      msg_error("I/O error occurred while writing",
                evt_tag_int("fd", self->super.transport->fd),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                evt_tag_id(MSG_IO_ERROR_WRITING),
                NULL);
      return LPS_ERROR;
    }

  return LPS_SUCCESS;
}

static LogProtoStatus
log_proto_text_client_post(LogProto *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed)
{
  // Non syslog protocol
  return log_proto_client_post_writer(s, logmsg, msg, msg_len, consumed, FALSE);
}

static LogProtoStatus
log_proto_framed_client_post_writer(LogProto *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed)
{
  // Syslog protocol
  return log_proto_client_post_writer(s, logmsg, msg, msg_len, consumed, TRUE);
}

static void
log_proto_text_client_free(LogProto *s)
{
  LogProtoTextClient *self = (LogProtoTextClient *)s;
  if (self->partial == NULL && (self->super.flags & LPBS_KEEP_ONE) && self->acked == 1)
    {
      log_proto_ack_msg(&self->super,1);
    }
}

LogProto *
log_proto_text_client_new(LogTransport *transport)
{
  LogProtoTextClient *self = g_new0(LogProtoTextClient, 1);

  self->super.prepare = log_proto_text_client_prepare;
  self->super.flush = log_proto_text_client_flush;
  self->super.post = log_proto_text_client_post;
  self->super.free_fn = log_proto_text_client_free;
  self->super.transport = transport;
  self->super.convert = (GIConv) -1;
  self->acked = 0;
  return &self->super;
}

typedef struct _LogProtoFileWriter
{
  LogProto super;
  guchar *partial;
  gsize partial_len, partial_pos;
  gint buf_size;
  gint buf_count;
  gint fd;
  gint sum_len;
  struct iovec buffer[0];
} LogProtoFileWriter;

/*
 * log_proto_file_writer_flush:
 *
 * this function flushes the file output buffer
 * it is called either form log_proto_file_writer_post (normal mode: the buffer is full)
 * or from log_proto_flush (foced flush: flush time, exit, etc)
 *
 */
static LogProtoStatus
log_proto_file_writer_flush(LogProto *s)
{
  LogProtoFileWriter *self = (LogProtoFileWriter *)s;
  gint rc, i, i0, sum, ofs, pos;

  /* we might be called from log_writer_deinit() without having a buffer at all */

  if (self->buf_count == 0)
    return LPS_SUCCESS;

  /* lseek() is used instead of O_APPEND, as on NFS  O_APPEND performs
   * poorly, as reported on the mailing list 2008/05/29 */

  lseek(self->fd, 0, SEEK_END);
  rc = writev(self->fd, self->buffer, self->buf_count);

  if (rc < 0)
    {
      if (errno != EINTR)
        {
          msg_error("I/O error occurred while writing",
                    evt_tag_int("fd", self->super.transport->fd),
                    evt_tag_errno(EVT_TAG_OSERROR, errno),
                    evt_tag_id(MSG_IO_ERROR_WRITING),
                    NULL);
          /*
           * EAGAIN is the only possible value
           * if we want to try to write the messages again the writer has to be woken up
           * so the last message won't be consumed and writer will try to send it again
           */
          self->buf_count--;
          return LPS_ERROR;
        }
      return LPS_SUCCESS;
    }
  else if (rc != self->sum_len)
    {
      /* partial success: not everything has been written out */
      /* look for the first chunk that has been cut */
      sum = self->buffer[0].iov_len; /* sum is the cumulated length of the already processed items */
      i = 0;
      while (rc > sum)
        sum += self->buffer[++i].iov_len;
      self->partial_len = sum - rc; /* this is the length of the first non-written chunk */
      i0 = i;
      ++i;
      /* add the lengths of the following messages */
      while (i < self->buf_count)
        self->partial_len += self->buffer[i++].iov_len;
      /* allocate and copy the remaning data */
      self->partial = (guchar *)g_malloc(self->partial_len);
      ofs = sum - rc; /* the length of the remaning (not processed) chunk in the first message */
      pos = self->buffer[i0].iov_len - ofs;
      memcpy(self->partial, self->buffer[i0].iov_base + pos, ofs);
      i = i0 + 1;
      while (i < self->buf_count)
        {
          memcpy(self->partial + ofs, self->buffer[i].iov_base, self->buffer[i].iov_len);
          ofs += self->buffer[i].iov_len;
          ++i;
        }
      self->partial_pos = 0;
    }

  /* free the previous message strings (the remaning part has been copied to the partial buffer) */
  for (i = 0; i < self->buf_count; ++i)
    g_free(self->buffer[i].iov_base);
  self->buf_count = 0;
  self->sum_len = 0;

  return LPS_SUCCESS;
}

/*
 * log_proto_file_writer_post:
 * @msg: formatted log message to send (this might be consumed by this function)
 * @msg_len: length of @msg
 * @consumed: pointer to a gboolean that gets set if the message was consumed by this function
 * @error: error information, if any
 *
 * This function posts a message to the log transport, performing buffering
 * of partially sent data if needed. The return value indicates whether we
 * successfully sent this message, or if it should be resent by the caller.
 **/
static LogProtoStatus
log_proto_file_writer_post(LogProto *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed)
{
  LogProtoFileWriter *self = (LogProtoFileWriter *)s;
  gint rc;
  LogProtoStatus result;

  *consumed = FALSE;
  if (self->buf_count >= self->buf_size)
    {
      rc = log_proto_file_writer_flush(s);
      if (rc != LPS_SUCCESS || self->buf_count >= self->buf_size)
        {
          /* don't consume a new message if flush failed, or even after the flush we don't have any free slots */
          return rc;
        }
    }

  if (self->partial)
    {
      /* there is still some data from the previous file writing process */
      gint len = self->partial_len - self->partial_pos;

      rc = write(self->fd, self->partial + self->partial_pos, len);
      if (rc < 0)
        {
          goto write_error;
        }
      else if (rc != len)
        {
          self->partial_pos += rc;
          return LPS_SUCCESS;
        }
      else
        {
          g_free(self->partial);
          self->partial = NULL;
          /* NOTE: we return here to give a chance to the framed protocol to send the frame header. */
          return LPS_SUCCESS;
        }
    }

  /* register the new message */
  self->buffer[self->buf_count].iov_base = (void *) msg;
  self->buffer[self->buf_count].iov_len = msg_len;
  ++self->buf_count;
  self->sum_len += msg_len;
  *consumed = TRUE;
  result = LPS_SUCCESS;
  log_proto_ack_msg(&self->super,1);
  if (self->buf_count == self->buf_size)
    {
      /* we have reached the max buffer size -> we need to write the messages */
      result = log_proto_file_writer_flush(s);
      if (result != LPS_SUCCESS)
        {
          *consumed = FALSE;
        }
    }
  return result;

write_error:
  if (errno != EAGAIN && errno != EINTR)
    {
      msg_error("I/O error occurred while writing",
                evt_tag_int("fd", self->super.transport->fd),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                evt_tag_id(MSG_IO_ERROR_WRITING),
                NULL);
      return LPS_ERROR;
    }

  return LPS_SUCCESS;
}

static gboolean
log_proto_file_writer_prepare(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  LogProtoFileWriter *self = (LogProtoFileWriter *) s;

  *fd = self->super.transport->fd;
  *cond = self->super.transport->cond;

  /* if there's no pending I/O in the transport layer, then we want to do a write */
  if (*cond == 0)
    *cond = G_IO_OUT;
  return self->buf_count > 0 || self->partial;
}

LogProto *
log_proto_file_writer_new(LogTransport *transport, gint flush_lines)
{
  if (flush_lines == 0)
    /* the flush-lines option has not been specified, use a default value */
    flush_lines = 1;
#ifdef IOV_MAX
  if (flush_lines > IOV_MAX)
    /* limit the flush_lines according to the current platform */
    flush_lines = IOV_MAX;
#endif

  /* allocate the structure with the proper number of items at the end */
  LogProtoFileWriter *self = (LogProtoFileWriter *)g_malloc0(sizeof(LogProtoFileWriter) + sizeof(struct iovec)*flush_lines);

  self->fd = transport->fd;
  self->buf_size = flush_lines;
  self->super.prepare = log_proto_file_writer_prepare;
  self->super.post = log_proto_file_writer_post;
  self->super.flush = log_proto_file_writer_flush;
  self->super.transport = transport;
  self->super.convert = (GIConv) -1;
  return &self->super;
}

typedef struct _AckDataFileState
{
  AckDataBase super;
  union{
    struct{
      gint32 pending_buffer_pos;
      gint64 pending_raw_stream_pos;
      gint32 pending_raw_buffer_size;
    }file_state;
    char other_state[MAX_STATE_DATA_LENGTH];
  };
} AckDataFileState;

typedef struct _LogProtoBufferedServerState
{
  /* NOTE: that if you add/remove structure members you have to update
   * the byte order swap code in LogProtoTextServer for mulit-byte
   * members. */
  BaseState super;
  guint8 raw_buffer_leftover_size;
  guint8 __padding1[1];
  guint32 buffer_pos;
  guint32 pending_buffer_end;
  guint32 buffer_size;
  guint32 buffer_cached_eol;
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
  guint32 run_id;
} LogProtoBufferedServerState;

typedef struct _LogProtoBufferedServer LogProtoBufferedServer;
struct _LogProtoBufferedServer
{
  LogProto super;
  gboolean (*fetch_from_buf)(LogProtoBufferedServer *self, const guchar *buffer_start, gsize buffer_bytes, const guchar **msg, gsize *msg_len, gboolean flush_the_rest);
  gint (*read_data)(LogProtoBufferedServer *self, guchar *buf, gsize len, GSockAddr **sa);

  LogProtoBufferedServerState *state1;
  PersistState *persist_state;
  StateHandler *state_handler;

  gint max_buffer_size;
  gint init_buffer_size;
  guchar *buffer;
  GSockAddr *prev_saddr;
  LogProtoStatus status;
  gboolean wait_for_prefix; /* This boolean tell to us that we are waiting for prefix or garbage (used only in case multi line messages */
};

void
log_proto_buffered_server_state_swap_bytes(LogProtoBufferedServerState *state)
{
  state->buffer_pos = GUINT32_SWAP_LE_BE(state->buffer_pos);
  state->pending_buffer_pos = GUINT32_SWAP_LE_BE(state->pending_buffer_pos);
  state->pending_buffer_end = GUINT32_SWAP_LE_BE(state->pending_buffer_end);
  state->buffer_size = GUINT32_SWAP_LE_BE(state->buffer_size);
  state->buffer_cached_eol = GUINT32_SWAP_LE_BE(state->buffer_cached_eol);
  state->raw_stream_pos = GUINT64_SWAP_LE_BE(state->raw_stream_pos);
  state->raw_buffer_size = GUINT32_SWAP_LE_BE(state->raw_buffer_size);
  state->pending_raw_stream_pos = GUINT64_SWAP_LE_BE(state->pending_raw_stream_pos);
  state->pending_raw_buffer_size = GUINT32_SWAP_LE_BE(state->pending_raw_buffer_size);
  state->file_size = GUINT64_SWAP_LE_BE(state->file_size);
  state->file_inode = GUINT64_SWAP_LE_BE(state->file_inode);
  state->run_id = GUINT32_SWAP_LE_BE(state->run_id);
}

void
log_proto_buffered_server_state_correct_endianess(LogProtoBufferedServerState *state)
{
  if ((state->super.big_endian && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
      (!state->super.big_endian && G_BYTE_ORDER == G_BIG_ENDIAN))
     {
        /* byte order conversion in order to avoid the hassle with
           scattered byte order conversions in the code */
        state->super.big_endian = !state->super.big_endian;
        log_proto_buffered_server_state_swap_bytes(state);
    }
}

NameValueContainer *
log_proto_buffered_server_state_handler_dump(StateHandler *self)
{
  NameValueContainer *result = name_value_container_new();

  LogProtoBufferedServerState *state = (LogProtoBufferedServerState *)state_handler_get_state(self);
  LogProtoBufferedServerState *data = g_new0(LogProtoBufferedServerState, 1);

  memcpy(data, state, sizeof(LogProtoBufferedServerState));
  state_handler_put_state(self);

  name_value_container_add_int(result, "version", data->super.version);
  name_value_container_add_boolean(result, "big_endian",data->super.big_endian);
  name_value_container_add_int(result, "raw_buffer_leftover_size", data->raw_buffer_leftover_size);
  name_value_container_add_int(result, "buffer_pos", data->buffer_pos);
  name_value_container_add_int(result, "pending_buffer_end", data->pending_buffer_end);
  name_value_container_add_int(result, "buffer_size", data->buffer_size);
  name_value_container_add_int(result, "buffer_cached_eol", data->buffer_cached_eol);
  name_value_container_add_int(result, "pending_buffer_pos", data->pending_buffer_pos);
  name_value_container_add_int64(result, "raw_stream_pos", data->raw_stream_pos);
  name_value_container_add_int64(result, "pending_raw_stream_pos", data->pending_raw_stream_pos);
  name_value_container_add_int(result, "raw_buffer_size", data->raw_buffer_size);
  name_value_container_add_int(result, "pending_raw_buffer_size" ,data->pending_raw_buffer_size);
  name_value_container_add_int64(result, "file_size", data->file_size);
  name_value_container_add_int64(result, "file_inode", data->file_inode);
  name_value_container_add_int(result, "run_id", data->run_id);

  g_free(data);
  return result;
}

PersistEntryHandle
log_proto_buffered_server_state_handler_alloc_state(StateHandler *self)
{
  self->size = sizeof(LogProtoBufferedServerState);
  return persist_state_alloc_entry(self->persist_state, self->name, self->size);
}

gboolean
log_proto_buffered_server_state_handler_is_valid_state(NameValueContainer *container)
{
  gboolean result = TRUE;
  result = result && name_value_container_is_name_exists(container, "version");
  result = result && name_value_container_is_name_exists(container, "big_endian");
  result = result && name_value_container_is_name_exists(container, "raw_buffer_leftover_size");
  result = result && name_value_container_is_name_exists(container, "buffer_pos");
  result = result && name_value_container_is_name_exists(container, "pending_buffer_end");
  result = result && name_value_container_is_name_exists(container, "buffer_size");
  result = result && name_value_container_is_name_exists(container, "buffer_cached_eol");
  result = result && name_value_container_is_name_exists(container, "pending_buffer_pos");
  result = result && name_value_container_is_name_exists(container, "raw_stream_pos");
  result = result && name_value_container_is_name_exists(container, "pending_raw_stream_pos");
  result = result && name_value_container_is_name_exists(container, "raw_buffer_size");
  result = result && name_value_container_is_name_exists(container, "pending_raw_buffer_size");
  result = result && name_value_container_is_name_exists(container, "file_size");
  result = result && name_value_container_is_name_exists(container, "file_inode");
  result = result && name_value_container_is_name_exists(container, "run_id");
  return result;
}

gboolean
log_proto_buffered_server_state_handler_load(StateHandler *self, NameValueContainer *container, GError **error)
{
  LogProtoBufferedServerState *new_state = NULL;
  if (!log_proto_buffered_server_state_handler_is_valid_state(container))
    {
      if (error)
        {
          *error = g_error_new(1, 2, "Some member is missing from the given state");
        }
      return FALSE;
    }
  if (!state_handler_entry_exists(self))
    {
      if (!state_handler_alloc_state(self))
        {
          if (error)
            {
              *error = g_error_new(1, 3, "Can't allocate new state");
            }
          return FALSE;
        }
    }
  new_state = state_handler_get_state(self);
  new_state->super.version = name_value_container_get_int(container, "version");
  new_state->super.big_endian = name_value_container_get_boolean(container, "big_endian");
  new_state->raw_buffer_leftover_size = name_value_container_get_int(container, "raw_buffer_leftover_size");
  new_state->buffer_pos = name_value_container_get_int(container, "buffer_pos");
  new_state->pending_buffer_end =  name_value_container_get_int(container, "pending_buffer_end");
  new_state->buffer_size = name_value_container_get_int(container, "buffer_size");
  new_state->buffer_cached_eol = name_value_container_get_int(container, "buffer_cached_eol");
  new_state->pending_buffer_pos = name_value_container_get_int(container, "pending_buffer_pos");
  new_state->raw_stream_pos = name_value_container_get_int64(container, "raw_stream_pos");
  new_state->pending_raw_stream_pos = name_value_container_get_int64(container, "pending_raw_stream_pos");
  new_state->raw_buffer_size = name_value_container_get_int(container, "raw_buffer_size");
  new_state->pending_raw_buffer_size = name_value_container_get_int(container, "pending_raw_buffer_size");
  new_state->file_size = name_value_container_get_int64(container, "file_size");
  new_state->file_inode = name_value_container_get_int64(container, "file_inode");
  new_state->run_id = name_value_container_get_int(container, "run_id");

  if ((new_state->super.big_endian && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
      (!new_state->super.big_endian && G_BYTE_ORDER == G_BIG_ENDIAN))
    {
      log_proto_buffered_server_state_swap_bytes(new_state);
    }
  state_handler_put_state(self);
  return TRUE;
}

StateHandler*
log_proto_buffered_server_state_handler_new(PersistState *persist_state, const gchar *name)
{
  StateHandler *self = g_new0(StateHandler, 1);
  state_handler_init(self, persist_state, name);
  self->dump_state = log_proto_buffered_server_state_handler_dump;
  self->alloc_state = log_proto_buffered_server_state_handler_alloc_state;
  self->load_state = log_proto_buffered_server_state_handler_load;
  return self;
}

static inline gboolean
log_proto_regexec(pcre *regex, const gchar *line, gint line_len, gint32 *first_offset, gint32 *end_offset)
{
  gint rc;
  const gchar *buf;
  int ovector[30] ={0};

  APPEND_ZERO(buf, line, line_len);
  rc = pcre_exec(regex, NULL, buf, line_len, 0, 0, ovector, 10);
  if (rc >= 0)
    {
      if (first_offset)
        *first_offset = ovector[0];
      if (end_offset)
        *end_offset = ovector[1];
    }
  return (rc >= 0);
}

/*
 * log_proto_get_fixed_encoding_scale:
 *
 * This function returns the number of bytes of a single character in the
 * encoding specified by the @encoding parameter, provided it is listed in
 * the limited set hard-wired in the fixed_encodings array above.
 *
 * syslog-ng sometimes needs to calculate the size of the original, raw data
 * that relates to its already utf8 converted input buffer.  For that the
 * slow solution is to actually perform the utf8 -> raw conversion, however
 * since we don't really need the actual conversion, just the size of the
 * data in bytes we can be faster than that by multiplying the number of
 * input characters with the size of the character in the known
 * fixed-length-encodings in the list above.
 *
 * This function returns 0 if the encoding is not known, in which case the
 * slow path is to be executed.
 */
static gint
log_proto_get_char_size_for_fixed_encoding(const gchar *encoding)
{
  static struct
  {
    const gchar *prefix;
    gint scale;
  } fixed_encodings[] = {
    { "ascii", 1 },
    { "us-ascii", 1 },
    { "iso-8859", 1 },
    { "iso8859", 1 },
    { "latin", 1 },
    { "ucs2", 2 },
    { "ucs-2", 2 },
    { "ucs4", 4 },
    { "ucs-4", 4 },
    { "koi", 1 },
    { "unicode", 2 },
    { "windows", 1 },
    { "wchar_t", sizeof(wchar_t) },
    { NULL, 0 }
  };
  gint scale = 0;
  gint i;

  for (i = 0; fixed_encodings[i].prefix; i++)
   {
     if (strncasecmp(encoding, fixed_encodings[i].prefix, strlen(fixed_encodings[i].prefix)) == 0)
       {
         scale = fixed_encodings[i].scale;
         break;
       }
   }
  return scale;
}

/*
 * NOTE: It is not allowed to synchronize with the main thread between
 * get_state & put_state, see persist_state_map_entry() for more details.
 */
static LogProtoBufferedServerState *
log_proto_buffered_server_get_state(LogProtoBufferedServer *self)
{
  if (self->persist_state)
    {
      g_assert(self->state_handler != 0);
      return (LogProtoBufferedServerState *)state_handler_get_state(self->state_handler);
    }
  if (G_UNLIKELY(!self->state1))
    {
      self->state1 = g_new0(LogProtoBufferedServerState, 1);
    }
  return self->state1;
}

static void
log_proto_buffered_server_put_state(LogProtoBufferedServer *self)
{
  if (self->state_handler)
    state_handler_put_state(self->state_handler);
}

static gboolean
log_proto_buffered_server_convert_from_raw(LogProtoBufferedServer *self, const guchar *raw_buffer, gsize raw_buffer_len)
{
  /* some data was read */
  gsize avail_in = raw_buffer_len;
  gsize avail_out;
  gchar *out;
  gint  ret = -1;
  gboolean success = FALSE;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(self);

  do
    {
      avail_out = state->buffer_size - state->pending_buffer_end;
      out = (gchar *) self->buffer + state->pending_buffer_end;

      ret = g_iconv(self->super.convert, (gchar **) &raw_buffer, &avail_in, (gchar **) &out, &avail_out);
      if (ret == (gsize) -1)
        {
          switch (errno)
            {
            case EINVAL:
              /* Incomplete text, do not report an error, rather try to read again */
              state->pending_buffer_end = state->buffer_size - avail_out;

              if (avail_in > 0)
                {
                  if (avail_in > sizeof(state->raw_buffer_leftover))
                    {
                      msg_error("Invalid byte sequence, the remaining raw buffer is larger than the supported leftover size",
                                evt_tag_str("encoding", self->super.encoding),
                                evt_tag_int("avail_in", avail_in),
                                evt_tag_int("leftover_size", sizeof(state->raw_buffer_leftover)),
                                evt_tag_id(MSG_CHAR_ENCODING_FAILED),
                                NULL);
                      goto error;
                    }
                  memcpy(state->raw_buffer_leftover, raw_buffer, avail_in);
                  state->raw_buffer_leftover_size = avail_in;
                  state->pending_raw_buffer_size -= avail_in;
                  msg_trace("Leftover characters remained after conversion, delaying message until another chunk arrives",
                            evt_tag_str("encoding", self->super.encoding),
                            evt_tag_int("avail_in", avail_in),
                            NULL);
                  goto success;
                }
              break;
            case E2BIG:
              state->pending_buffer_end = state->buffer_size - avail_out;
              /* extend the buffer */

              if (state->buffer_size < self->max_buffer_size)
                {
                  state->buffer_size *= 2;
                  if (state->buffer_size > self->max_buffer_size)
                    state->buffer_size = self->max_buffer_size;

                  self->buffer = g_realloc(self->buffer, state->buffer_size);

                  /* recalculate the out pointer, and add what we have now */
                  ret = -1;
                }
              else
                {
                  msg_error("Incoming byte stream requires a too large conversion buffer, probably invalid character sequence",
                            evt_tag_str("encoding", self->super.encoding),
                            evt_tag_printf("buffer", "%.*s", (gint) state->pending_buffer_end, self->buffer),
                            evt_tag_id(MSG_CHAR_ENCODING_FAILED),
                            NULL);
                  goto error;
                }
              break;
            case EILSEQ:
            default:
              msg_notice("Invalid byte sequence or other error while converting input, skipping character",
                         evt_tag_str("encoding", self->super.encoding),
                         evt_tag_printf("char", "0x%02x", *(guchar *) raw_buffer),
                         evt_tag_id(MSG_INVALID_ENCODING_SEQ),
                         NULL);
              goto error;
            }
        }
      else
        {
          state->pending_buffer_end = state->buffer_size - avail_out;
        }
    }
  while (avail_in > 0);

 success:
  success = TRUE;
 error:
  log_proto_buffered_server_put_state(self);
  return success;
}

static void
log_proto_buffered_server_apply_state(LogProto *s, StateHandler *state_handler)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *)s;
  struct stat st;
  LogProtoBufferedServerState *state;
  gint fd;
  gboolean new_run;
  guint64 raw_stream_pos, raw_buffer_size, buffer_pos;

  fd = self->super.transport->fd;
  if (self->state_handler)
      {
        state_handler_free(self->state_handler);
      }
  self->state_handler = state_handler;

  if (fstat(fd, &st) < 0)
    return;

  state = log_proto_buffered_server_get_state(self);

  new_run = state->run_id != g_run_id;

  if (!self->buffer)
    {
      self->buffer = g_malloc(state->buffer_size);
    }

  if (new_run)
  {
    state->pending_raw_stream_pos = state->raw_stream_pos;
    state->pending_raw_buffer_size = state->raw_buffer_size;
    state->pending_buffer_pos = state->buffer_pos;
    state->buffer_cached_eol = 0;
  }

  raw_stream_pos = state->pending_raw_stream_pos;
  raw_buffer_size = state->pending_raw_buffer_size;
  buffer_pos = state->pending_buffer_pos;

  state->pending_buffer_end = 0;
#ifdef _WIN32
  state->file_inode = 1;
  st.st_ino = 1;
#endif

  if (state->file_inode &&
      state->file_inode == st.st_ino &&
      state->file_size <= st.st_size &&
      raw_stream_pos <= st.st_size)
    {
      lseek(fd, raw_stream_pos, SEEK_SET);
    }
  else
    {
      if (state->file_inode)
        {
          /* the stored state does not match the current file */
          msg_notice("The current log file has a mismatching size/inode information, restarting from the beginning",
                     evt_tag_str("state", state_handler_get_persist_name(state_handler)),
                     evt_tag_int("stored_inode", state->file_inode),
                     evt_tag_int("cur_file_inode", st.st_ino),
                     evt_tag_int("stored_size", state->file_size),
                     evt_tag_int("cur_file_size", st.st_size),
                     evt_tag_int("raw_stream_pos", state->raw_stream_pos),
                     evt_tag_int("pending_raw_stream_pos", state->pending_raw_stream_pos),
                     NULL);
        }
      goto error;
    }
  if (raw_buffer_size)
    {
      gssize rc;
      guchar *pending_raw_buffer;

      if (!self->super.encoding)
        {
          /* no conversion, we read directly into our buffer */
          if (raw_buffer_size > state->buffer_size)
            {
              msg_notice("Invalid LogProtoBufferedServerState.raw_buffer_size, larger than buffer_size and no encoding is set, restarting from the beginning",
                         evt_tag_str("state", state_handler_get_persist_name(state_handler)),
                         evt_tag_int("raw_buffer_size", state->raw_buffer_size),
                         evt_tag_int("pending_raw_buffer_size", state->pending_raw_buffer_size),
                         evt_tag_int("buffer_size", state->buffer_size),
                         evt_tag_int("init_buffer_size", self->init_buffer_size),
                         NULL);
              goto error;
            }
          pending_raw_buffer = self->buffer;
        }
      else
        {
          gint scale = 0;
          scale = log_proto_get_char_size_for_fixed_encoding(self->super.encoding);
          if (scale && raw_buffer_size > (scale *state->buffer_size))
            {
              msg_notice("Invalid LogProtoBufferedServerState.raw_buffer_size, larger than init_buffer_size, restarting from the beginning",
                         evt_tag_str("state", state_handler_get_persist_name(state_handler)),
                         evt_tag_int("raw_buffer_size", state->raw_buffer_size),
                         evt_tag_int("pending_raw_buffer_size", state->pending_raw_buffer_size),
                         evt_tag_int("init_buffer_size", self->init_buffer_size),
                         NULL);
              goto error;
            }
          pending_raw_buffer = g_alloca(raw_buffer_size);
        }

      rc = log_transport_read(self->super.transport, pending_raw_buffer, raw_buffer_size, NULL);

      if (rc != raw_buffer_size)
        {
         if (rc != -1 && rc > buffer_pos)
         {
               raw_buffer_size = rc;
         }
         else
         {
          msg_notice("Error re-reading buffer contents of the file to be continued, restarting from the beginning",
                     evt_tag_str("state", state_handler_get_persist_name(state_handler)),
                    evt_tag_int("read",rc),
                    evt_tag_int("had to read",raw_buffer_size),
                    evt_tag_errno("Error",errno),
                     NULL);
          goto error;
         }
        }

      state->pending_buffer_end = 0;
      if (self->super.encoding)
        {
          if (!log_proto_buffered_server_convert_from_raw(self, pending_raw_buffer, rc))
            {
              msg_notice("Error re-converting buffer contents of the file to be continued, restarting from the beginning",
                         evt_tag_str("state", state_handler_get_persist_name(state_handler)),
                         NULL);
              goto error;
            }
        }
      else
        {
          state->pending_buffer_end += rc;
        }

      if (buffer_pos > state->pending_buffer_end || (gint32) state->buffer_cached_eol > (gint32) state->pending_buffer_end)
        {
          msg_notice("Converted buffer contents is smaller than the current buffer position, starting from the beginning of the buffer, some lines may be duplicated",
                     evt_tag_str("state", state_handler_get_persist_name(state_handler)),
                     evt_tag_int("buffer_pos", state->buffer_pos),
                     evt_tag_int("pending_buffer_pos", state->pending_buffer_pos),
                     evt_tag_int("pending_buffer_end", state->pending_buffer_end),
                     evt_tag_int("buffer_cached_eol", state->buffer_cached_eol),
                     NULL);
          buffer_pos = state->buffer_cached_eol = 0;
        }
    }
  else
    {
      /* although we do have buffer position information, but the
       * complete contents of the buffer is already processed, instead
       * of reading and then dropping it, position the file after the
       * indicated block */

      raw_stream_pos += raw_buffer_size;
      buffer_pos = state->pending_buffer_end = 0;
      lseek(fd, raw_stream_pos, SEEK_SET);
    }
  goto exit;

 error:
  buffer_pos = 0;
  raw_stream_pos = 0;
  raw_buffer_size = 0;
  state->pending_buffer_end = 0;
  state->buffer_cached_eol = 0;
  state->raw_stream_pos = 0;
  state->raw_buffer_size = 0;
  state->raw_buffer_leftover_size = 0;
  lseek(fd, 0, SEEK_SET);

 exit:
  state->file_inode = st.st_ino;
  state->file_size = st.st_size;
  state->pending_buffer_pos = buffer_pos;
  state->pending_raw_stream_pos = raw_stream_pos;
  state->pending_raw_buffer_size = raw_buffer_size;

  if (new_run)
  {
    if (state->pending_buffer_end != 0 && state->buffer_cached_eol == 0)
      {
        state->buffer_cached_eol = (guint32) -1;
      }
    state->run_id = g_run_id;
  }

  state = NULL;
  log_proto_buffered_server_put_state(self);
}

static PersistEntryHandle
log_proto_buffered_server_alloc_state(LogProtoBufferedServer *self, StateHandler *handler)
{
  LogProtoBufferedServerState *state;
  PersistEntryHandle handle;

  handle = state_handler_alloc_state(handler);
  if (handle)
    {
      state = (LogProtoBufferedServerState *)state_handler_get_state(handler);

      state->super.version = 1;
      state->super.big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
      state->run_id = g_run_id;

      state_handler_put_state(handler);

    }
  return handle;
}

static gboolean
log_proto_buffered_server_convert_state(LogProtoBufferedServer *self, guint8 persist_version, gpointer old_state, gsize old_state_size, LogProtoBufferedServerState *state)
{
  if (persist_version < 3)
    {
      msg_error("Internal error restoring log reader state, stored data is too old",
                   evt_tag_int("version", persist_version),
                   evt_tag_id(MSG_CANT_LOAD_READER_STATE),
                   NULL);
      return FALSE;
    }
  else if (persist_version == 3)
    {
      SerializeArchive *archive;
      guint32 read_length;
      gint64 cur_size;
      gint64 cur_inode;
      gint64 cur_pos;
      guint16 version;
      gchar *buffer;
      gsize buffer_len;

      cur_inode = -1;
      cur_pos = 0;
      cur_size = 0;
      archive = serialize_buffer_archive_new(old_state, old_state_size);

      /* NOTE: the v23 conversion code adds an extra length field which we
       * need to read out. */
      g_assert(serialize_read_uint32(archive, &read_length) && read_length == old_state_size - sizeof(read_length));

      /* original v3 format starts here */
      if (!serialize_read_uint16(archive, &version) || version != 0)
        {
          msg_error("Internal error restoring log reader state, stored data has incorrect version",
                    evt_tag_int("version", version),
                    evt_tag_id(MSG_CANT_LOAD_READER_STATE),
                    NULL);
          goto error_converting_v3;
        }

      if (!serialize_read_uint64(archive, (guint64 *) &cur_pos) ||
          !serialize_read_uint64(archive, (guint64 *) &cur_inode) ||
          !serialize_read_uint64(archive, (guint64 *) &cur_size))
        {
          msg_error("Internal error restoring information about the current file position, restarting from the beginning",
                    evt_tag_id(MSG_CANT_LOAD_READER_STATE),
                    NULL);
          goto error_converting_v3;
        }

      if (!serialize_read_uint16(archive, &version) || version != 0)
        {
          msg_error("Internal error, protocol state has incorrect version",
                    evt_tag_int("version", version),
                    evt_tag_id(MSG_CANT_LOAD_READER_STATE),
                    NULL);
          goto error_converting_v3;
        }

      if (!serialize_read_cstring(archive, &buffer, &buffer_len))
        {
          msg_error("Internal error, error reading buffer contents",
                    evt_tag_int("version", version),
                    evt_tag_id(MSG_CANT_LOAD_READER_STATE),
                    NULL);
          goto error_converting_v3;
        }

      if (!self->buffer || state->buffer_size < buffer_len)
        {
          gsize buffer_size = MAX(self->init_buffer_size, buffer_len);
          self->buffer = g_realloc(self->buffer, buffer_size);
        }
      serialize_archive_free(archive);

      memcpy(self->buffer, buffer, buffer_len);
      state->buffer_pos = 0;
      state->pending_buffer_end = buffer_len;
      g_free(buffer);

      state->super.version = 1;
      state->file_inode = cur_inode;
      state->raw_stream_pos = cur_pos;
      state->file_size = cur_size;
      state->super.big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
      state->run_id = g_run_id-1;
      return TRUE;
    error_converting_v3:
      serialize_archive_free(archive);
    }
  return FALSE;
}

StateHandler *
log_proto_buffered_server_realloc_state(LogProtoBufferedServer *self, StateHandler *state_handler)
{
  StateHandler *new_state_handler = log_proto_buffered_server_state_handler_new(state_handler->persist_state, state_handler->name);
  if (!log_proto_buffered_server_alloc_state(self, new_state_handler))
    {
      state_handler_free(new_state_handler);
      new_state_handler = NULL;
    }
  return new_state_handler;
}

void
log_proto_buffered_server_fallback_non_persistent(LogProtoBufferedServer *self)
{
  LogProtoBufferedServerState *new_state = g_new0(LogProtoBufferedServerState, 1);
  new_state->pending_raw_stream_pos = 0;
  new_state->file_inode = 0;
  new_state->file_size = 0;
  self->state1 = new_state;
}


void
log_proto_buffered_server_reset_state(LogProto *self)
{
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state((LogProtoBufferedServer *)self);
  if (state)
    {
      gsize buffer_size = state->buffer_size;
      memset(state, 0, sizeof(LogProtoBufferedServerState));

      state->super.big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
      state->buffer_size = buffer_size;
      log_proto_buffered_server_put_state((LogProtoBufferedServer *)self);
    }
}

gboolean
log_proto_buffered_server_update_server_state(LogProtoBufferedServer *self, StateHandler *state_handler, gint persist_version)
{
  gpointer new_state = NULL;
  gpointer old_state;
  gboolean success;
  StateHandler *new_state_handler;

  old_state = state_handler_get_state(state_handler);
  new_state_handler = log_proto_buffered_server_realloc_state(self, state_handler);
  if (!new_state_handler)
    {
      state_handler_free(state_handler);
      log_proto_buffered_server_fallback_non_persistent(self);
      return FALSE;
    }
  new_state = state_handler_get_state(new_state_handler);

  success = log_proto_buffered_server_convert_state(self, persist_version, old_state, state_handler->size, new_state);

  state_handler_put_state(state_handler);
  state_handler_put_state(new_state_handler);

  /* we're using the newly allocated state structure regardless if
   * conversion succeeded. If the conversion went wrong then
   * new_state is still in its freshly initialized form since the
   * convert function will not touch the state in the error
   * branches.
   */
  state_handler_free(state_handler);

  log_proto_apply_state(&self->super, new_state_handler);
  return success;
}

StateHandler *
log_proto_buffered_server_get_new_state_handler(LogProtoBufferedServer *self, StateHandler *state_handler)
{
  StateHandler *new_state_handler;
  LogProtoBufferedServerState *new_state = NULL;
  gpointer old_state;

  old_state = state_handler_get_state(state_handler);

  new_state_handler = log_proto_buffered_server_realloc_state(self, state_handler);
  if (!new_state_handler)
    {
      state_handler_free(state_handler);
      log_proto_buffered_server_fallback_non_persistent(self);
      return NULL;
    }
  new_state = (LogProtoBufferedServerState *)state_handler_get_state(new_state_handler);

  memcpy(new_state,old_state,state_handler->size);
  new_state->super.version = 1;
  new_state->run_id = g_run_id - 1;



  /* we're using the newly allocated state structure regardless if
   * conversion succeeded. If the conversion went wrong then
   * new_state is still in its freshly initialized form since the
   * convert function will not touch the state in the error
   * branches.
   */
  state_handler_free(state_handler);
  return new_state_handler;
}



gboolean
log_proto_buffered_server_handle_actual_state_version(LogProtoBufferedServer *self, StateHandler *state_handler)
{
  LogProtoBufferedServerState *state = (LogProtoBufferedServerState *)state_handler_get_state(state_handler);

  if (state->super.version == 0)
    {
      state_handler = log_proto_buffered_server_get_new_state_handler(self, state_handler);
      if (!state_handler)
        {
          return FALSE;
        }
      state = (LogProtoBufferedServerState *)state_handler_get_state(state_handler);
    }
  if (state->super.version == 1)
    {
      log_proto_buffered_server_state_correct_endianess(state);
      log_proto_apply_state(&self->super, state_handler);
      return TRUE;
    }
  g_assert_not_reached();
  return FALSE;
}

gboolean
log_proto_buffered_server_create_new_state(LogProtoBufferedServer *self, StateHandler *state_handler)
{
  if (!log_proto_buffered_server_alloc_state(self, state_handler))
    {
      log_proto_buffered_server_fallback_non_persistent(self);
      state_handler_free(state_handler);
      return FALSE;
    }
  log_proto_apply_state(&self->super, state_handler);
  return TRUE;
}

gboolean
log_proto_buffered_server_restart_with_state(LogProto *s, PersistState *persist_state, const gchar *persist_name)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;
  guint8 persist_version;
  StateHandler *state_handler = NULL;
  guint8 state_version = 0;

  self->persist_state = persist_state;
  state_handler = log_proto_buffered_server_state_handler_new(persist_state, persist_name);
  if (!state_handler_entry_exists(state_handler))
    {
      return log_proto_buffered_server_create_new_state(self, state_handler);
    }
  persist_version = state_handler_get_persist_version(state_handler);
  if (persist_version < 4)
    {
      return log_proto_buffered_server_update_server_state(self, state_handler, persist_version);
    }
  else if (persist_version == 4)
    {
       LogProtoBufferedServerState *state = (LogProtoBufferedServerState *)state_handler_get_state(state_handler);
       state_version = state->super.version;
       if (state->super.version < 2)
         {
           return log_proto_buffered_server_handle_actual_state_version(self, state_handler);
         }
    }
  msg_error("Internal error restoring log reader state, stored data is too new",
             evt_tag_int("version", persist_version),
             evt_tag_int("state_version", state_version),
             evt_tag_id(MSG_CANT_LOAD_READER_STATE),
             NULL);
  log_proto_buffered_server_create_new_state(self, state_handler);
  return FALSE;
}

static gboolean
log_proto_buffered_server_prepare(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;

  *fd = self->super.transport->fd;
  *cond = self->super.transport->cond;

  /* if there's no pending I/O in the transport layer, then we want to do a read */
  if (*cond == 0)
    *cond = G_IO_IN;

  return FALSE;
}


static gint
log_proto_buffered_server_read_data(LogProtoBufferedServer *self, guchar *buf, gsize len, GSockAddr **sa)
{
  return log_transport_read(self->super.transport, buf, len, sa);
}

static LogProtoStatus
log_proto_buffered_server_fetch_from_buf(LogProtoBufferedServer *self, const guchar **msg, gsize *msg_len, gboolean flush_the_rest)
{
  gsize buffer_bytes;
  const guchar *buffer_start;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(self);
  gboolean success = FALSE;

  buffer_start = self->buffer + state->pending_buffer_pos;
  buffer_bytes = state->pending_buffer_end - state->pending_buffer_pos;

  if (buffer_bytes == 0)
    {
      /* if buffer_bytes is zero bytes, it means that we completely
       * processed our buffer without having a fraction of a line still
       * there.  It is important to reset
       * pending_buffer_pos/pending_buffer_end to zero as the caller assumes
       * that if we return no message from the buffer, then buffer_pos is
       * _zero_.
       */

      if (G_UNLIKELY(self->super.flags & LPBS_POS_TRACKING))
        {
          state->pending_raw_stream_pos += state->pending_raw_buffer_size;
          state->pending_raw_buffer_size = 0;
        }
      state->pending_buffer_pos = state->pending_buffer_end = 0;
      goto exit;
    }

  success = self->fetch_from_buf(self, buffer_start, buffer_bytes, msg, msg_len, flush_the_rest);
 exit:
  log_proto_buffered_server_put_state(self);
  return success;
}

/**
 * Returns: TRUE to indicate success, FALSE otherwise. The returned
 * msg can be NULL even if no failure occurred.
 **/
static LogProtoStatus
log_proto_buffered_server_fetch(LogProto *s, const guchar **msg, gsize *msg_len, GSockAddr **sa, gboolean *may_read, gboolean flush)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;
  gint rc;
  guchar *raw_buffer = NULL;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(self);
  LogProtoStatus result = self->status;
  LogProtoServerOptions *options = (LogProtoServerOptions *)self->super.options;

  if (G_UNLIKELY(!self->buffer))
    {
      self->buffer = g_malloc(self->init_buffer_size);
      state->buffer_size = self->init_buffer_size;
    }

  if (sa)
    *sa = NULL;

  if (state->buffer_cached_eol == (guint32) -1)
    state->buffer_cached_eol = 0;

  if (flush)
  {
    /* in multiline, the fill has been restarted: return the remaining message content */
    if (options && options->opts.garbage_matcher && self->wait_for_prefix)
      {
        *msg = NULL;
        *msg_len = 0;
      }
    else
      {
        *msg = self->buffer + state->pending_buffer_pos;
        *msg_len = state->pending_buffer_end - state->pending_buffer_pos;
        state->pending_buffer_pos = state->pending_buffer_end;
        if (sa && self->prev_saddr)
          *sa = g_sockaddr_ref(self->prev_saddr);
        if (*msg_len == 0)
          {
            *msg = NULL;
          }
        else if (options && options->opts.garbage_matcher)
          {
            if (log_proto_regexec(options->opts.garbage_matcher, (const gchar *)*msg, *msg_len, NULL, NULL))
              {
                *msg = NULL;
                *msg_len = 0;
              }
          }
        else
          {
            /* eliminate the current content's closing LF */
            if((*msg)[(*msg_len)-1]=='\n')
              {
                *msg_len -= 1;
              }
          }
      }
    result = LPS_SUCCESS;
    /* goto exit and do the free methods */
    goto exit;
  }

  if (self->status != LPS_SUCCESS)
    {
      goto exit;
    }

  if (log_proto_buffered_server_fetch_from_buf(self, msg, msg_len, FALSE))
    {
      if (sa && self->prev_saddr)
        *sa = g_sockaddr_ref(self->prev_saddr);
      goto exit;
    }

  /* ok, no more messages in the buffer, read a chunk */
  while (*may_read)
    {
      gint avail;

      if (self->super.flags & LPBS_NOMREAD)
        *may_read = FALSE;

      /* read the next chunk to be processed */

      if (self->prev_saddr)
        {
          /* new chunk of data, potentially new sockaddr, forget the previous value */
          g_sockaddr_unref(self->prev_saddr);
          self->prev_saddr = NULL;
        }

      if (!self->super.encoding)
        {
          /* no conversion, we read directly into our buffer */
          raw_buffer = self->buffer + state->pending_buffer_end;
          avail = state->buffer_size - state->pending_buffer_end;
        }
      else
        {
          /* if conversion is needed, we first read into an on-stack
           * buffer, and then convert it into our internal buffer */

          raw_buffer = g_alloca(self->init_buffer_size + state->raw_buffer_leftover_size);
          memcpy(raw_buffer, state->raw_buffer_leftover, state->raw_buffer_leftover_size);
          avail = self->init_buffer_size;
        }

      rc = self->read_data(self, raw_buffer + state->raw_buffer_leftover_size, avail, sa);
      if (sa && *sa)
        self->prev_saddr = *sa;
      if (rc < 0)
        {
          if (errno == EAGAIN)
            {
              /* ok we don't have any more data to read, return to main poll loop */
              break;
            }
          else
            {
              /* an error occurred while reading */
              msg_error("I/O error occurred while reading",
                        evt_tag_int(EVT_TAG_FD, self->super.transport->fd),
                        evt_tag_errno(EVT_TAG_OSERROR, errno),
                        evt_tag_id(MSG_IO_ERROR_READING),
                        NULL);

              /* we set self->status explicitly as we want to return
               * LPS_ERROR on the _next_ invocation, not now */
              self->status = LPS_ERROR;
              if (log_proto_buffered_server_fetch_from_buf(self, msg, msg_len, TRUE))
                {
                  if (sa && self->prev_saddr)
                    *sa = g_sockaddr_ref(self->prev_saddr);
                  goto exit;
                }
              result = self->status;
              goto exit;
            }
        }
      else if (rc == 0)
        {
          if ((self->super.flags & LPBS_IGNORE_EOF) == 0)
            {
              /* EOF read */
              msg_verbose("EOF occurred while reading",
                          evt_tag_int(EVT_TAG_FD, self->super.transport->fd),
                          NULL);
              if (state->raw_buffer_leftover_size > 0)
                {
                  msg_error("EOF read on a channel with leftovers from previous character conversion, dropping input",
                            evt_tag_id(MSG_EOF_OCCURED),
                            NULL);
                  result = LPS_EOF;
                  goto exit;
                }
              self->status = LPS_EOF;
              if (log_proto_buffered_server_fetch_from_buf(self, msg, msg_len, TRUE))
                {
                  if (sa && self->prev_saddr)
                    *sa = g_sockaddr_ref(self->prev_saddr);
                  goto exit;
                }
              result = self->status;
              goto exit;
            }
          else
            {
              *msg = NULL;
              *msg_len = 0;
              goto exit;
            }
        }
      else
        {
          state->pending_raw_buffer_size += rc;
          rc += state->raw_buffer_leftover_size;
          state->raw_buffer_leftover_size = 0;

          if (self->super.encoding)
            {
              if (!log_proto_buffered_server_convert_from_raw(self, raw_buffer, rc))
                {
                  result = LPS_ERROR;
                  goto exit;
                }
            }
          else
            {
              state->pending_buffer_end += rc;
            }

          if (log_proto_buffered_server_fetch_from_buf(self, msg, msg_len, FALSE))
            {
              if (sa && self->prev_saddr)
                *sa = g_sockaddr_ref(self->prev_saddr);
              goto exit;
            }
        }
    }
 exit:

  /* result contains our result, but once an error happens, the error condition remains persistent */
  log_proto_buffered_server_put_state(self);
  if (result != LPS_SUCCESS)
    self->status = result;
  return result;
}

void
log_proto_buffered_server_get_state_info(LogProto *s,gpointer user_data)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(self);

  AckDataFileState *data_state=  (AckDataFileState*) user_data;

  data_state->file_state.pending_buffer_pos = state->pending_buffer_pos;
  data_state->file_state.pending_raw_stream_pos = state->pending_raw_stream_pos;
  data_state->file_state.pending_raw_buffer_size = state->pending_raw_buffer_size;
  data_state->super.persist_handle = self->state_handler ? state_handler_get_handle(self->state_handler) : 0;
  log_proto_buffered_server_put_state(self);
}

void
log_proto_buffered_server_queued(LogProto *s)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(self);
  if (state->pending_buffer_pos == state->pending_buffer_end)
    {
      state->pending_buffer_end = 0;
      state->pending_buffer_pos = 0;
      if (self->super.flags & LPBS_POS_TRACKING)
        {
              state->pending_raw_stream_pos += state->pending_raw_buffer_size;
              state->pending_raw_buffer_size = 0;
        }
    }
  log_proto_buffered_server_put_state(self);
  return;
}

gboolean
log_proto_buffered_server_ack(PersistState *persist_state,gpointer user_data, gboolean need_to_save)
{
  AckDataFileState *data_state = (AckDataFileState*) user_data;
  LogProtoBufferedServerState *state = NULL;
  if (!need_to_save)
    {
      return FALSE;
    }

  state = persist_state_map_entry(persist_state, data_state->super.persist_handle);

  state->buffer_pos = data_state->file_state.pending_buffer_pos;
  state->raw_stream_pos = data_state->file_state.pending_raw_stream_pos;
  state->raw_buffer_size = data_state->file_state.pending_raw_buffer_size;

  msg_trace("Last message got confirmed",
            evt_tag_int("raw_stream_pos", state->raw_stream_pos),
            evt_tag_int("raw_buffer_len", state->raw_buffer_size),
            evt_tag_int("buffer_pos", state->buffer_pos),
            evt_tag_int("buffer_end", state->pending_buffer_end),
            evt_tag_int("buffer_cached_eol", state->buffer_cached_eol),
            NULL);
  persist_state_unmap_entry(persist_state, data_state->super.persist_handle);
  return FALSE;
}

void
log_proto_buffered_server_free(LogProto *s)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;
  g_sockaddr_unref(self->prev_saddr);

  g_free(self->buffer);
  if (self->state1)
    {
      g_free(self->state1);
    }
  if (self->state_handler)
    {
      state_handler_free(self->state_handler);
    }
}

void
log_proto_buffered_server_init(LogProtoBufferedServer *self, LogTransport *transport, gint max_buffer_size, gint init_buffer_size, guint flags)
{
  self->super.prepare = log_proto_buffered_server_prepare;
  self->super.fetch = log_proto_buffered_server_fetch;
  self->super.queued = log_proto_buffered_server_queued;
  self->super.free_fn = log_proto_buffered_server_free;
  self->super.reset_state = log_proto_buffered_server_reset_state;
  self->super.get_state = log_proto_buffered_server_get_state_info;
  self->super.apply_state = log_proto_buffered_server_apply_state;
  //self->super.ack = log_proto_buffered_server_ack;
  self->super.transport = transport;
  self->super.convert = (GIConv) -1;
  self->super.restart_with_state = log_proto_buffered_server_restart_with_state;
  self->read_data = log_proto_buffered_server_read_data;

  self->super.flags = flags;

  self->init_buffer_size = init_buffer_size;
  self->max_buffer_size = max_buffer_size;
}

typedef struct _LogProtoTextServer
{
  LogProtoBufferedServer super;
  GIConv reverse_convert;
  gchar *reverse_buffer;
  gsize reverse_buffer_len;
  gint convert_scale;
  gboolean has_to_update; /* This boolean indicate, that we has to drop the read message (between garbage and prefix) */
} LogProtoTextServer;

/**
 * This function is called in cases when several files are continously
 * polled for changes.  Whenever the caller would like to switch to another
 * file, it will call this function to check whether it should be allowed to do so.
 *
 * This function returns true if the current state of this LogProto would
 * allow preemption, e.g.  the contents of the current buffer can be
 * discarded.
 **/
gboolean
log_proto_text_server_is_preemptable(LogProto *s)
{
  LogProtoTextServer *self = (LogProtoTextServer *) s;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(&self->super);
  gboolean preemptable;

  preemptable = (state->buffer_cached_eol == 0);
  log_proto_buffered_server_put_state(&self->super);
  return preemptable;
}

static gboolean
log_proto_text_server_prepare(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  LogProtoTextServer *self = (LogProtoTextServer *) s;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(&self->super);
  gboolean avail;

  if (log_proto_buffered_server_prepare(s, fd, cond, timeout))
    {
      log_proto_buffered_server_put_state(&self->super);
      return TRUE;
    }

  avail = (state->buffer_cached_eol != 0);
  log_proto_buffered_server_put_state(&self->super);
  return avail;
}

/**
 * Find the character terminating the buffer.
 *
 * NOTE: when looking for the end-of-message here, it either needs to be
 * terminated via NUL or via NL, when terminating via NL we have to make
 * sure that there's no NUL left in the message. This function iterates over
 * the input data and returns a pointer to the first occurence of NL or NUL.
 *
 * It uses an algorithm similar to what there's in libc memchr/strchr.
 *
 * NOTE: find_eom is not static as it is used by a unit test program.
 **/

#if GLIB_SIZEOF_LONG == GLIB_SIZEOF_VOID_P
#define LONGDEF gulong
#elif GLIB_SIZEOF_VOID_P == 4
#define LONGDEF guint32
#elif GLIB_SIZEOF_VOID_P == 8
#define LONGDEF guint64
#else
#error "Unsupported word length, only 32 or 64 bit platforms are suppoted"
#endif

const guchar *
find_eom(const guchar *s, gsize n)
{
  const guchar *char_ptr;
  const LONGDEF *longword_ptr;
  LONGDEF longword, magic_bits, charmask;
  gchar c;

  c = '\n';

  /* align input to long boundary */
  for (char_ptr = s; n > 0 && ((LONGDEF) char_ptr & (sizeof(longword) - 1)) != 0; ++char_ptr, n--)
    {
      if (*char_ptr == c || *char_ptr == '\0')
        return char_ptr;
    }

  longword_ptr = (LONGDEF *) char_ptr;

#if GLIB_SIZEOF_VOID_P == 8
  magic_bits = 0x7efefefefefefeffL;
#elif GLIB_SIZEOF_VOID_P == 4
  magic_bits = 0x7efefeffL;
#else
  #error "unknown architecture"
#endif
  memset(&charmask, c, sizeof(charmask));

  while (n > sizeof(longword))
    {
      longword = *longword_ptr++;
      if ((((longword + magic_bits) ^ ~longword) & ~magic_bits) != 0 ||
          ((((longword ^ charmask) + magic_bits) ^ ~(longword ^ charmask)) & ~magic_bits) != 0)
        {
          gint i;

          char_ptr = (const guchar *) (longword_ptr - 1);

          for (i = 0; i < sizeof(longword); i++)
            {
              if (*char_ptr == c || *char_ptr == '\0')
                return char_ptr;
              char_ptr++;
            }
        }
      n -= sizeof(longword);
    }

  char_ptr = (const guchar *) longword_ptr;

  while (n-- > 0)
    {
      if (*char_ptr == c || *char_ptr == '\0')
        return char_ptr;
      ++char_ptr;
    }

  return NULL;
}

/*
 * returns the number of bytes that represent the UTF8 encoding buffer
 * in the original encoding that the user specified.
 *
 * NOTE: this is slow, but we only call this for the remainder of our
 * buffer (e.g. the partial line at the end of our last chunk of read
 * data). Also, this is only invoked if the file uses an encoding.
 */
static gsize
log_proto_text_server_get_raw_size_of_buffer(LogProtoTextServer *self, const guchar *buffer, gsize buffer_len)
{
  gchar *out;
  const guchar *in;
  gsize avail_out, avail_in;
  gint ret;

  if (self->reverse_convert == ((GIConv) -1) && !self->convert_scale)
    {
      /* try to speed up raw size calculation by recognizing the most
       * prominent character encodings and in the case the encoding
       * uses fixed size characters set that in self->convert_scale,
       * which in turn will speed up the reversal of the UTF8 buffer
       * size to raw buffer sizes.
       */
      self->convert_scale = log_proto_get_char_size_for_fixed_encoding(self->super.super.encoding);
      if (self->convert_scale == 0)
        {
          /* this encoding is not known, do the conversion for real :( */
          self->reverse_convert = g_iconv_open(self->super.super.encoding, "utf-8");
        }
    }

  if (self->convert_scale)
    return g_utf8_strlen((gchar *) buffer, buffer_len) * self->convert_scale;

  if (self->reverse_buffer_len < buffer_len * 6)
    {
      /* we free and malloc, since we never need the data still in reverse buffer */
      g_free(self->reverse_buffer);
      self->reverse_buffer_len = buffer_len * 6;
      self->reverse_buffer = g_malloc(buffer_len * 6);
    }

  avail_out = self->reverse_buffer_len;
  out = self->reverse_buffer;

  avail_in = buffer_len;
  in = buffer;

  ret = g_iconv(self->reverse_convert, (gchar **) &in, &avail_in, &out, &avail_out);
  if (ret == (gsize) -1)
    {
      /* oops, we cannot reverse that we ourselves converted to UTF-8,
       * this is simply impossible, but never say never */
      msg_error("Internal error, couldn't reverse the internal UTF8 string to the original encoding",
                evt_tag_printf("buffer", "%.*s", (gint) buffer_len, buffer),
                evt_tag_id(MSG_REVERSE_CONVERTING_FAILED),
                NULL);
      return 0;
    }
  else
    {
      return self->reverse_buffer_len - avail_out;
    }
}

const guchar *
find_multi_line_eom(LogProtoTextServer *self, const guchar *buffer_start, gsize buffer_bytes, pcre *multi_line_prefix_parser, pcre *multi_line_garbage_parser, guint32 *new_buffer_pos)
{
  const guchar *line = buffer_start;
  const guchar *next_line = buffer_start;
  gint32 line_len = buffer_bytes;
  gint32 found_pos = 0;
  gint32 end_pos = 0;
  if (new_buffer_pos)
    *new_buffer_pos = 0;
  if (!self->super.wait_for_prefix)
    {
      /* If garbage is defined we are searching it */
      if (multi_line_garbage_parser)
        {
          next_line = find_eom(line, buffer_bytes);
          while(next_line)
            {
              line_len = next_line - line;
              next_line++;
              if (log_proto_regexec(multi_line_garbage_parser, (gchar*)line, line_len, &found_pos, &end_pos))
                {
                  self->super.wait_for_prefix = TRUE;
                  if (new_buffer_pos)
                    *new_buffer_pos = end_pos + line - buffer_start;
                  line += found_pos;
                  break;
                }
              line = next_line;
              next_line = find_eom(line, buffer_bytes - (line - buffer_start));
            }
          if (!next_line)
            {
              line_len = buffer_bytes - (line - buffer_start);
              if (!log_proto_regexec(multi_line_garbage_parser, (gchar*)line, line_len, &found_pos, &end_pos))
                {
                  line = NULL;
                }
              else
                {
                  self->super.wait_for_prefix = TRUE;
                  if (new_buffer_pos)
                    *new_buffer_pos = end_pos + line - buffer_start;
                  line+= found_pos;
                }
            }
        }
      /* Next message is start with prefix */
      else
        {
          /* this is the first line */
          next_line = find_eom(line, buffer_bytes);
          if (next_line)
            {
              line = next_line;
              next_line = find_eom(line, buffer_bytes - (line - buffer_start));
              while(next_line)
                {
                  line_len = next_line - line;
                  next_line++;
                  if (log_proto_regexec(multi_line_prefix_parser, (gchar*)line, line_len, &found_pos, NULL))
                    {
                      break;
                    }
                  line = next_line;
                  next_line = find_eom(line, buffer_bytes - (line - buffer_start));
                }
              if (!next_line)
                {
                  line_len = buffer_bytes - (line - buffer_start);
                  if (!log_proto_regexec(multi_line_prefix_parser, (gchar*)line, line_len, &found_pos, NULL))
                    {
                      line = NULL;
                    }
                }
            }
          else
            {
              line = NULL;
            }
        }
    }
  else
    {
      /* found garbage and wait for prefix */
      next_line = find_eom(line, buffer_bytes);
      while(next_line)
        {
          line_len = next_line - line;
          next_line++;
          if (log_proto_regexec(multi_line_prefix_parser, (gchar*)line, line_len, NULL, NULL))
            {
              self->super.wait_for_prefix = FALSE;
              break;
            }
          line = next_line;
          next_line = find_eom(line, buffer_bytes - (line - buffer_start));
        }
      if (!next_line)
        {
          line_len = buffer_bytes - (line - buffer_start);
          if (!log_proto_regexec(multi_line_prefix_parser, (gchar*)line, line_len, &found_pos, NULL))
            {
              line = NULL;
            }
          else
            {
              self->super.wait_for_prefix = FALSE;
            }
        }
    }
  return line;
}

/**
 * log_proto_text_server_fetch_from_buf:
 * @self: LogReader instance
 * @saddr: socket address to be assigned to new messages (consumed!)
 * @flush: whether to flush the input buffer
 * @msg_counter: the number of messages processed in the current poll iteration
 *
 * Returns TRUE if a message was found in the buffer, FALSE if we need to read again.
 **/
static gboolean
log_proto_text_server_fetch_from_buf(LogProtoBufferedServer *s, const guchar *buffer_start, gsize buffer_bytes, const guchar **msg, gsize *msg_len, gboolean flush_the_rest)
{
  LogProtoTextServer *self = (LogProtoTextServer *) s;
  const guchar *eol;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(&self->super);
  gboolean result = FALSE;
  gboolean waited_for_prefix;
  guint32 new_pos = 0;
  LogProtoServerOptions *options = (LogProtoServerOptions *)self->super.super.options;

  if (flush_the_rest)
    {
      /*
       * we are set to packet terminating mode or the connection is to
       * be teared down and we have partial data in our buffer.
       */
      if (options && options->opts.prefix_matcher && self->super.wait_for_prefix)
        {
          *msg = NULL;
          *msg_len = 0;
        }
      else
        {
          *msg = buffer_start;
          *msg_len = buffer_bytes;
          state->pending_buffer_pos = state->pending_buffer_end;
        }
      goto success;
    }

  if (state->buffer_cached_eol)
    {
      /* previous invocation was nice enough to save a cached EOL
       * pointer, no need to look it up again */

      eol = self->super.buffer + state->buffer_cached_eol;
      state->buffer_cached_eol = 0;
    }
  else
    {
      if (!options || !options->opts.prefix_matcher)
        {
          eol = find_eom(buffer_start, buffer_bytes);
        }
      else
        {
          waited_for_prefix = self->super.wait_for_prefix;
          eol = find_multi_line_eom(self,
                                    buffer_start,
                                    buffer_bytes,
                                    options->opts.prefix_matcher,
                                    options->opts.garbage_matcher,
                                    &new_pos);
          state->pending_buffer_pos += new_pos;
          /* Found Garbage and not update the pending buffer pos later */
          if (new_pos)
            self->has_to_update = FALSE;
          if (waited_for_prefix && !self->super.wait_for_prefix)
            {
              /* this buffer part has to be dropped */
              state->pending_buffer_pos = eol - self->super.buffer;
              log_proto_buffered_server_put_state(&self->super);
              return log_proto_text_server_fetch_from_buf(&self->super,
                                                           buffer_start,
                                                           buffer_bytes,
                                                           msg,
                                                           msg_len,
                                                           flush_the_rest);
            }
        }
    }
  if ((!eol && (buffer_bytes == state->buffer_size)))
    {
      /* our buffer is full and no EOL was found */
      *msg_len = buffer_bytes;
      state->pending_buffer_pos = state->pending_buffer_end;
      *msg = buffer_start;
      goto success;
    }
  else if (!eol)
    {
      gsize raw_split_size;

      /* buffer is not full, but no EOL is present, move partial line
       * to the beginning of the buffer to make space for new data.
       */

      memmove(self->super.buffer, buffer_start, buffer_bytes);
      state->pending_buffer_pos = 0;
      state->pending_buffer_end = buffer_bytes;

      if (G_UNLIKELY(self->super.super.flags & LPBS_POS_TRACKING))
        {
          /* NOTE: we modify the current file position _after_ updating
             buffer_pos, since if we crash right here, at least we
             won't lose data on the next restart, but rather we
             duplicate some data */

          if (self->super.super.encoding)
            raw_split_size = log_proto_text_server_get_raw_size_of_buffer(self, buffer_start, buffer_bytes);
          else
            raw_split_size = buffer_bytes;

          state->pending_raw_stream_pos += (gint64) (state->pending_raw_buffer_size - raw_split_size);
          state->pending_raw_buffer_size = raw_split_size;

          msg_trace("Buffer split",
                    evt_tag_int("raw_split_size", raw_split_size),
                    evt_tag_int("buffer_bytes", buffer_bytes),
                    NULL);
        }
      goto exit;
    }
  else
    {
      const guchar *msg_end = eol;

      /* eol points at the newline character. end points at the
       * character terminating the line, which may be a carriage
       * return preceeding the newline. */

      while ((msg_end > buffer_start) && (msg_end[-1] == '\r' || msg_end[-1] == '\n' || msg_end[-1] == 0))
        msg_end--;

      *msg_len = msg_end - buffer_start;
      *msg = buffer_start;
      if (!options || !options->opts.prefix_matcher)
        {
          state->pending_buffer_pos = eol - self->super.buffer;
          state->pending_buffer_pos +=1;
        }
      else if (self->has_to_update)
        {
          state->pending_buffer_pos = eol - self->super.buffer;
        }

      if (state->pending_buffer_end != state->pending_buffer_pos)
        {
          const guchar *eom;
          /* store the end of the next line, it indicates whether we need
           * to read further data, or the buffer already contains a
           * complete line */
          if (!options || !options->opts.prefix_matcher)
            {
              eom = find_eom(self->super.buffer + state->pending_buffer_pos, state->pending_buffer_end - state->pending_buffer_pos);
            }
          else
            {
              waited_for_prefix = self->super.wait_for_prefix;
              eom = find_multi_line_eom(self,
                                        self->super.buffer + state->pending_buffer_pos,
                                        state->pending_buffer_end - state->pending_buffer_pos,
                                        options->opts.prefix_matcher,
                                        options->opts.garbage_matcher,
                                        &new_pos);
              state->pending_buffer_pos += new_pos;
              /* found garbage don't upgrade the pending buffer pos */
              if (new_pos)
                self->has_to_update = FALSE;
              if (eom && waited_for_prefix && !self->super.wait_for_prefix)
                {
                  /* this buffer part has to be dropped */
                  /* prefix found at eom */
                  /* lets find the garbage */
                  state->pending_buffer_pos = eom - self->super.buffer;
                  eom = find_multi_line_eom(self,
                                            self->super.buffer + state->pending_buffer_pos,
                                            state->pending_buffer_end - state->pending_buffer_pos,
                                            options->opts.prefix_matcher,
                                            options->opts.garbage_matcher,
                                            &new_pos);
                  self->has_to_update = TRUE;
                }
            }
          /*
           * If the wait_for_prefix is reset (e.g.: wildcard filesource file switch)
           * the buffer_cached eol won't move, but the pending_buffer_pos is moved over
           * the cached eol, and it cause buffer overread
           */
          if (eom && (eom > (self->super.buffer + state->pending_buffer_pos)))
            state->buffer_cached_eol = eom - self->super.buffer;
          else
            state->buffer_cached_eol = 0;
        }
      else
        {
          state->pending_buffer_pos = state->pending_buffer_end;
        }
      goto success;
    }
 success:
  result = TRUE;
 exit:
  log_proto_buffered_server_put_state(&self->super);
  return result;
}

static void
log_proto_text_server_get_info(LogProto *s,guint64 *pos)
{
  LogProtoTextServer *self =(LogProtoTextServer *)s;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(&self->super);

  if (pos)
    *pos = state->pending_raw_stream_pos + state->pending_buffer_pos;
  log_proto_buffered_server_put_state(&self->super);
}

void
log_proto_text_server_free(LogProto *p)
{
  LogProtoTextServer *self = (LogProtoTextServer *)p;
  if (self->reverse_convert != (GIConv) -1)
    g_iconv_close(self->reverse_convert);

  g_free(self->reverse_buffer);
  log_proto_buffered_server_free(&self->super.super);
}

static void
log_proto_text_server_init(LogProtoTextServer *self, LogTransport *transport, gint max_msg_size, guint flags)
{
  log_proto_buffered_server_init(&self->super, transport, max_msg_size * 6, max_msg_size, flags);
  self->super.fetch_from_buf = log_proto_text_server_fetch_from_buf;
  self->super.super.prepare = log_proto_text_server_prepare;
  self->super.super.get_info = log_proto_text_server_get_info;
  self->super.super.free_fn = log_proto_text_server_free;
  self->reverse_convert = (GIConv) -1;
  self->super.wait_for_prefix = FALSE;
  self->has_to_update = TRUE;
}

LogProto *
log_proto_text_server_new(LogTransport *transport, LogProtoServerOptions *soptions)
{
  LogProtoTextServer *self = g_new0(LogProtoTextServer, 1);
  if (soptions && soptions->opts.prefix_matcher)
    {
      self->super.super.is_multi_line = TRUE;
    }
  log_proto_text_server_init(self, transport, soptions->super.size,soptions->super.flags);
  return &self->super.super;
}

static void
log_proto_file_reader_init(LogProtoTextServer *self, LogTransport *transport, gint max_msg_size, guint flags)
{
  log_proto_text_server_init(self, transport, max_msg_size, flags);
  self->super.super.ack = log_proto_buffered_server_ack;
  self->super.super.is_preemptable = log_proto_text_server_is_preemptable;
}

LogProto *
log_proto_file_reader_new(LogTransport *transport, LogProtoServerOptions *soptions)
{
  LogProtoTextServer *self = g_new0(LogProtoTextServer, 1);
  if (soptions && soptions->opts.prefix_matcher)
    {
      self->super.super.is_multi_line = TRUE;
    }
  log_proto_file_reader_init(self, transport, soptions->super.size,soptions->super.flags);
  return &self->super.super;
}

/* proto that reads the stream in even sized chunks */
typedef struct _LogProtoRecordServer LogProtoRecordServer;
struct _LogProtoRecordServer
{
  LogProtoBufferedServer super;
  gsize record_size;
};

static gboolean
log_proto_record_server_fetch_from_buf(LogProtoBufferedServer *s, const guchar *buffer_start, gsize buffer_bytes, const guchar **msg, gsize *msg_len, gboolean flush_the_rest)
{
  LogProtoRecordServer *self = (LogProtoRecordServer *) s;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(s);
  const guchar *eol;

  if (!(self->super.super.flags & LPRS_BINARY))
    {
      eol = find_eom(buffer_start, buffer_bytes);
      *msg_len = (eol ? eol - buffer_start : buffer_bytes);
    }
  else
    {
      *msg_len = buffer_bytes;
    }
  state->pending_buffer_pos = state->pending_buffer_end;
  *msg = buffer_start;
  log_proto_buffered_server_put_state(s);
  return TRUE;
}

static gint
log_proto_record_server_read_data(LogProtoBufferedServer *s, guchar *buf, gsize len, GSockAddr **sa)
{
  LogProtoRecordServer *self = (LogProtoRecordServer *) s;
  gint rc;

  g_assert(len <= self->record_size);
  len = self->record_size;
  rc = log_transport_read(self->super.super.transport, buf, len, sa);
  if (rc > 0 && rc != self->record_size)
    {
      msg_error("Padding was set, and couldn't read enough bytes",
                evt_tag_int(EVT_TAG_FD, self->super.super.transport->fd),
                evt_tag_int("padding", self->record_size),
                evt_tag_int("read", rc),
                NULL);
      errno = EIO;
      return -1;
    }
  return rc;
}

LogProto *
log_proto_record_server_new(LogTransport *transport, gint record_size, guint flags)
{
  LogProtoRecordServer *self = g_new0(LogProtoRecordServer, 1);

  log_proto_buffered_server_init(&self->super, transport, record_size * 6, record_size, flags);
  self->super.fetch_from_buf = log_proto_record_server_fetch_from_buf;
  self->super.read_data = log_proto_record_server_read_data;
  self->record_size = record_size;
  return &self->super.super;
}

/* proto that reads the stream in even sized chunks */
typedef struct _LogProtoDGramServer LogProtoDGramServer;
struct _LogProtoDGramServer
{
  LogProtoBufferedServer super;
};

static gboolean
log_proto_dgram_server_fetch_from_buf(LogProtoBufferedServer *s, const guchar *buffer_start, gsize buffer_bytes, const guchar **msg, gsize *msg_len, gboolean flush_the_rest)
{
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(s);

  /*
   * we are set to packet terminating mode
   */
  *msg = buffer_start;
  *msg_len = buffer_bytes;
  state->pending_buffer_pos = state->pending_buffer_end;
  log_proto_buffered_server_put_state(s);
  return TRUE;
}

LogProto *
log_proto_dgram_server_new(LogTransport *transport, gint max_msg_size, guint flags)
{
  LogProtoRecordServer *self = g_new0(LogProtoRecordServer, 1);

  log_proto_buffered_server_init(&self->super, transport, max_msg_size * 6, max_msg_size, flags | LPBS_IGNORE_EOF);
  self->super.fetch_from_buf = log_proto_dgram_server_fetch_from_buf;
  return &self->super.super;
}

static LogProtoStatus
log_proto_framed_client_post(LogProto *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed)
{
  LogProtoFramedClient *self = (LogProtoFramedClient *) s;
  gint rc;

  if (msg_len > 9999999)
    {
      static const guchar *warn_msg;

      if (warn_msg != msg)
        {
          msg_warning("Error, message length too large for framed protocol, truncated",
                      evt_tag_int("length", msg_len),
                      NULL);
          warn_msg = msg;
        }
      msg_len = 9999999;
    }
  switch (self->state)
    {
    case LPFCS_FRAME_INIT:
      self->frame_hdr_len = g_snprintf(self->frame_hdr_buf, sizeof(self->frame_hdr_buf), "%" G_GSIZE_FORMAT" ", msg_len);
      self->frame_hdr_pos = 0;
      self->state = LPFCS_FRAME_SEND;
    case LPFCS_FRAME_SEND:
      rc = log_transport_write(s->transport, &self->frame_hdr_buf[self->frame_hdr_pos], self->frame_hdr_len - self->frame_hdr_pos);
      if (rc < 0)
        {
          if (errno != EAGAIN)
            {
              msg_error("I/O error occurred while writing",
                        evt_tag_int("fd", self->super.super.transport->fd),
                        evt_tag_errno(EVT_TAG_OSERROR, errno),
                        evt_tag_id(MSG_IO_ERROR_WRITING),
                        NULL);
              return LPS_ERROR;
            }
          break;
        }
      else
        {
          self->frame_hdr_pos += rc;
          if (self->frame_hdr_pos != self->frame_hdr_len)
            break;
          self->state = LPFCS_MESSAGE_SEND;
        }
    case LPFCS_MESSAGE_SEND:
      rc = log_proto_framed_client_post_writer(s, logmsg, msg, msg_len, consumed);

      /* NOTE: we don't check *consumed here, as we might have a pending
       * message in self->partial before we begin, in which case *consumed
       * will be FALSE. */

      if (rc == LPS_SUCCESS && self->super.partial == NULL)
        {
          self->state = LPFCS_FRAME_INIT;
        }
      return rc;
    default:
      g_assert_not_reached();
    }
  return LPS_SUCCESS;
}

LogProto *
log_proto_framed_client_new(LogTransport *transport)
{
  LogProtoFramedClient *self = g_new0(LogProtoFramedClient, 1);

  self->super.super.prepare = log_proto_text_client_prepare;
  self->super.super.post = log_proto_framed_client_post;
  self->super.super.flush = log_proto_text_client_flush;
  self->super.super.transport = transport;
  self->super.super.convert = (GIConv) -1;
  return &self->super.super;
}

#define LPFSS_FRAME_READ    0
#define LPFSS_MESSAGE_READ  1

#define LPFS_FRAME_BUFFER 10

typedef struct _LogProtoFramedServer
{
  LogProto super;
  gint state;

  guchar *buffer;
  gsize buffer_size, buffer_pos, buffer_end;
  gsize frame_len;
  gsize max_msg_size;
  gboolean half_message_in_buffer;
  GSockAddr *prev_saddr;
  LogProtoStatus status;
} LogProtoFramedServer;

static gboolean
log_proto_framed_server_prepare(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  LogProtoFramedServer *self = (LogProtoFramedServer *) s;

  *fd = self->super.transport->fd;
  *cond = self->super.transport->cond;

  /* there is a half message in our buffer so try to wait */
  if (!self->half_message_in_buffer)
    {
      if (self->buffer_pos != self->buffer_end)
        {
          /* we have a full message in our buffer so parse it without reading new data from the transport layer */
          return TRUE;
        }
    }

  /* if there's no pending I/O in the transport layer, then we want to do a read */
  if (*cond == 0)
    *cond = G_IO_IN;

  return FALSE;
}

static LogProtoStatus
log_proto_framed_server_fetch_data(LogProtoFramedServer *self, gboolean *may_read)
{
  gint rc;

  if (self->buffer_pos == self->buffer_end)
    self->buffer_pos = self->buffer_end = 0;

  if (self->buffer_size == self->buffer_end)
    {
      /* no more space in the buffer, we can't fetch further data. Move the
       * things we already have to the beginning of the buffer to make
       * space. */

      memmove(self->buffer, &self->buffer[self->buffer_pos], self->buffer_end - self->buffer_pos);
      self->buffer_end = self->buffer_end - self->buffer_pos;
      self->buffer_pos = 0;
    }

  if (!(*may_read))
    return LPS_SUCCESS;

  rc = log_transport_read(self->super.transport, &self->buffer[self->buffer_end], self->buffer_size - self->buffer_end, NULL);

  if (rc < 0)
    {
      if (errno != EAGAIN)
        {
          msg_error("Error reading frame header",
                    evt_tag_int("fd", self->super.transport->fd),
                    evt_tag_errno("error", errno),
                    NULL);
          return LPS_ERROR;
        }
      else
        {
          /* we need more data to parse this message but the data is not available yet */
          self->half_message_in_buffer = TRUE;
        }
    }
  else if (rc == 0)
    {
      msg_verbose("EOF occurred while reading",
                  evt_tag_int(EVT_TAG_FD, self->super.transport->fd),
                  NULL);
      return LPS_EOF;
    }
  else
    {
      self->buffer_end += rc;
    }
  return LPS_SUCCESS;

}

static gboolean
log_proto_framed_server_extract_frame_length(LogProtoFramedServer *self, gboolean *need_more_data)
{
  gint i;

  *need_more_data = TRUE;
  self->frame_len = 0;
  for (i = self->buffer_pos; i < self->buffer_end; i++)
    {
      if (isdigit(self->buffer[i]) && (i - self->buffer_pos < 10))
        {
          self->frame_len = self->frame_len * 10 + (self->buffer[i] - '0');
        }
      else if (self->buffer[i] == ' ')
        {
          *need_more_data = FALSE;
          self->buffer_pos = i + 1;
          return TRUE;
        }
      else
        {
          msg_error("Invalid frame header",
                    evt_tag_printf("header", "%.*s", (gint) (i - self->buffer_pos), &self->buffer[self->buffer_pos]),
                    NULL);
          return FALSE;
        }
    }
  /* couldn't extract frame header, no error but need more data */
  return TRUE;
}

static LogProtoStatus
log_proto_framed_server_fetch(LogProto *s, const guchar **msg, gsize *msg_len, GSockAddr **sa, gboolean *may_read, gboolean flush)
{
  LogProtoFramedServer *self = (LogProtoFramedServer *) s;
  LogProtoStatus status;
  gboolean try_read, need_more_data;

  if (sa)
    *sa = NULL;
  switch (self->state)
    {
    case LPFSS_FRAME_READ:

      try_read = TRUE;

    read_frame:
      if (!log_proto_framed_server_extract_frame_length(self, &need_more_data))
        {
          /* invalid frame header */
          return LPS_ERROR;
        }
      if (need_more_data && try_read)
        {
          status = log_proto_framed_server_fetch_data(self, may_read);
          if (status != LPS_SUCCESS)
            return status;
          try_read = FALSE;
          goto read_frame;
        }

      if (!need_more_data)
        {
          self->state = LPFSS_MESSAGE_READ;
          if (self->frame_len > self->max_msg_size)
            {
              msg_error("Incoming frame larger than log_msg_size()",
                        evt_tag_int("log_msg_size", self->buffer_size - LPFS_FRAME_BUFFER),
                        evt_tag_int("frame_length", self->frame_len),
                        NULL);
              return LPS_ERROR;
            }
          if (self->buffer_pos + self->frame_len > self->buffer_size)
            {
              /* message would be too large to fit into the buffer at
               * buffer_pos, we need to move data to the beginning of
               * the buffer to make space, and once we are at it, move
               * to the beginning to make space for the maximum number
               * of messages before the next shift */
              memmove(self->buffer, &self->buffer[self->buffer_pos], self->buffer_end - self->buffer_pos);
              self->buffer_end = self->buffer_end - self->buffer_pos;
              self->buffer_pos = 0;
            }
          goto read_message;
        }
      break;
    case LPFSS_MESSAGE_READ:

      try_read = TRUE;
    read_message:
      /* NOTE: here we can assume that the complete message fits into
       * the buffer because of the checks/move operation in the
       * LPFSS_FRAME_READ state */
      if (self->buffer_end - self->buffer_pos >= self->frame_len)
        {
          /* ok, we already have the complete message */

          *msg = &self->buffer[self->buffer_pos];
          *msg_len = self->frame_len;
          self->buffer_pos += self->frame_len;
          self->state = LPFSS_FRAME_READ;

          /* we have the full message here so reset the half message flag */
          self->half_message_in_buffer = FALSE;
          return LPS_SUCCESS;
        }
      if (try_read)
        {
          status = log_proto_framed_server_fetch_data(self, may_read);
          if (status != LPS_SUCCESS)
            return status;
          try_read = FALSE;
          goto read_message;
        }
      break;
    default:
      break;
    }
  return LPS_SUCCESS;
}

static void
log_proto_framed_server_free(LogProto *s)
{
  LogProtoFramedServer *self = (LogProtoFramedServer *) s;
  g_free(self->buffer);
}

LogProto *
log_proto_framed_server_new(LogTransport *transport, gint max_msg_size)
{
  LogProtoFramedServer *self = g_new0(LogProtoFramedServer, 1);

  self->super.prepare = log_proto_framed_server_prepare;
  self->super.fetch = log_proto_framed_server_fetch;
  self->super.free_fn = log_proto_framed_server_free;
  self->super.transport = transport;
  self->super.convert = (GIConv) -1;
  /* max message + frame header */
  self->max_msg_size = max_msg_size;
  self->buffer_size = max_msg_size + LPFS_FRAME_BUFFER;
  self->buffer = g_malloc(self->buffer_size);
  self->half_message_in_buffer = FALSE;
  return &self->super;
}

LogProto *
log_proto_framed_client_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg)
{
  return log_proto_framed_client_new(transport);
}

LogProto *
log_proto_text_client_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg)
{
  return log_proto_text_client_new(transport);
}

LogProto *
log_proto_file_writer_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg)
{
  return log_proto_file_writer_new(transport,options->super.size);
}

LogProto *
log_proto_record_client_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg)
{
  return log_proto_text_client_new(transport);
}

LogProto *
log_proto_dgram_client_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg)
{
  return log_proto_text_client_new(transport);
}

LogProto *
log_proto_framed_server_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg)
{
  return log_proto_framed_server_new(transport,options->super.size);
}

LogProto *
log_proto_text_server_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg)
{
  LogProtoServerOptions *soptions = (LogProtoServerOptions *)options;
  return log_proto_text_server_new(transport,soptions);
}

LogProto *
log_proto_file_reader_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg)
{
  LogProtoServerOptions *soptions = (LogProtoServerOptions *)options;
  return log_proto_file_reader_new(transport,soptions);
}

LogProto *
log_proto_record_server_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg)
{
  return log_proto_record_server_new(transport, options->super.size,options->super.flags);
}

LogProto *
log_proto_dgram_server_new_plugin(LogTransport *transport,LogProtoOptions *options,GlobalConfig *cfg)
{
  return log_proto_dgram_server_new(transport, options->super.size,options->super.flags);
}
