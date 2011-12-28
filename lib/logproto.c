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


#include "logproto.h"
#include "messages.h"
#include "persist-state.h"
#include "compat.h"

#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <limits.h>

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
  log_transport_free(s->transport);
  g_free(s);
}


typedef struct _LogProtoTextClient
{
  LogProto super;
  guchar *partial;
  gsize partial_len, partial_pos;
} LogProtoTextClient;

static gboolean
log_proto_text_client_prepare(LogProto *s, gint *fd, GIOCondition *cond)
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
log_proto_text_client_flush(LogProto *s)
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
log_proto_text_client_post(LogProto *s, guchar *msg, gsize msg_len, gboolean *consumed)
{
  LogProtoTextClient *self = (LogProtoTextClient *) s;
  gint rc;

  /* NOTE: the client does not support charset conversion for now */
  g_assert(self->super.convert == (GIConv) -1);

  *consumed = FALSE;
  rc = log_proto_flush(s);
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
  return LPS_SUCCESS;

 write_error:
  if (errno != EAGAIN && errno != EINTR)
    {
      msg_error("I/O error occurred while writing",
                evt_tag_int("fd", self->super.transport->fd),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return LPS_ERROR;
    }

  return LPS_SUCCESS;
}

LogProto *
log_proto_text_client_new(LogTransport *transport)
{
  LogProtoTextClient *self = g_new0(LogProtoTextClient, 1);

  self->super.prepare = log_proto_text_client_prepare;
  self->super.flush = log_proto_text_client_flush;
  self->super.post = log_proto_text_client_post;
  self->super.transport = transport;
  self->super.convert = (GIConv) -1;
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
      if (errno != EAGAIN && errno != EINTR)
        {
          msg_error("I/O error occurred while writing",
                    evt_tag_int("fd", self->super.transport->fd),
                    evt_tag_errno(EVT_TAG_OSERROR, errno),
                    NULL);
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
log_proto_file_writer_post(LogProto *s, guchar *msg, gsize msg_len, gboolean *consumed)
{
  LogProtoFileWriter *self = (LogProtoFileWriter *)s;
  gint rc;

  if (self->buf_count >= self->buf_size)
    {
      rc = log_proto_file_writer_flush(s);
      if (rc != LPS_SUCCESS || self->buf_count >= self->buf_size)
        {
          /* don't consume a new message if flush failed, or even after the flush we don't have any free slots */
          return rc;
        }
    }

  *consumed = FALSE;
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

  if (self->buf_count == self->buf_size)
    {
      /* we have reached the max buffer size -> we need to write the messages */
      return log_proto_file_writer_flush(s);
    }

  return LPS_SUCCESS;

write_error:
  if (errno != EAGAIN && errno != EINTR)
    {
      msg_error("I/O error occurred while writing",
                evt_tag_int("fd", self->super.transport->fd),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return LPS_ERROR;
    }

  return LPS_SUCCESS;
}

static gboolean
log_proto_file_writer_prepare(LogProto *s, gint *fd, GIOCondition *cond)
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



typedef struct _LogProtoBufferedServerState
{
  /* NOTE: that if you add/remove structure members you have to update
   * the byte order swap code in LogProtoFileReader for mulit-byte
   * members. */

  guint8 version;

  /* this indicates that the members in the struct are stored in
   * big-endian byte order. if the byte ordering of the struct doesn't
   * match the current CPU byte ordering, then the members are
   * byte-swapped when the state is loaded.
   */
  guint8 big_endian:1;
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
} LogProtoBufferedServerState;

typedef struct _LogProtoBufferedServer LogProtoBufferedServer;
struct _LogProtoBufferedServer
{
  LogProto super;
  gboolean (*fetch_from_buf)(LogProtoBufferedServer *self, const guchar *buffer_start, gsize buffer_bytes, const guchar **msg, gsize *msg_len, gboolean flush_the_rest);
  gint (*read_data)(LogProtoBufferedServer *self, guchar *buf, gsize len, GSockAddr **sa);

  LogProtoBufferedServerState *state1;
  PersistState *persist_state;
  PersistEntryHandle persist_handle;

  gint max_buffer_size;
  gint init_buffer_size;
  guchar *buffer;
  GSockAddr *prev_saddr;
  LogProtoStatus status;
};

static LogProtoBufferedServerState *
log_proto_buffered_server_get_state(LogProtoBufferedServer *self)
{
  if (self->persist_state)
    {
      g_assert(self->persist_handle != 0);
      return persist_state_map_entry(self->persist_state, self->persist_handle);
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
  if (self->persist_state && self->persist_handle)
    persist_state_unmap_entry(self->persist_state, self->persist_handle);
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
                                NULL);
                      goto error;
                    }
                  memcpy(state->raw_buffer_leftover, raw_buffer, avail_in);
                  state->raw_buffer_leftover_size = avail_in;
                  state->raw_buffer_size -= avail_in;
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

              if ((state->buffer_size < self->max_buffer_size))
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
                            NULL);
                  goto error;
                }
              break;
            case EILSEQ:
            default:
              msg_notice("Invalid byte sequence or other error while converting input, skipping character",
                         evt_tag_str("encoding", self->super.encoding),
                         evt_tag_printf("char", "0x%02x", *(guchar *) raw_buffer),
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
log_proto_buffered_server_apply_state(LogProtoBufferedServer *self, PersistEntryHandle handle, const gchar *persist_name)
{
  struct stat st;
  gint64 ofs = 0;
  LogProtoBufferedServerState *state;
  gint fd;

  fd = self->super.transport->fd;
  self->persist_handle = handle;

  if (fstat(fd, &st) < 0)
    return;

  state = log_proto_buffered_server_get_state(self);

  if (!self->buffer)
    {
      self->buffer = g_malloc(state->buffer_size);
    }
  state->pending_buffer_end = 0;

  if (state->file_inode &&
      state->file_inode == st.st_ino &&
      state->file_size <= st.st_size &&
      state->raw_stream_pos <= st.st_size)
    {
      ofs = state->raw_stream_pos;

      lseek(fd, ofs, SEEK_SET);
    }
  else
    {
      if (state->file_inode)
        {
          /* the stored state does not match the current file */
          msg_notice("The current log file has a mismatching size/inode information, restarting from the beginning",
                     evt_tag_str("state", persist_name),
                     evt_tag_int("stored_inode", state->file_inode),
                     evt_tag_int("cur_file_inode", st.st_ino),
                     evt_tag_int("stored_size", state->file_size),
                     evt_tag_int("cur_file_size", st.st_size),
                     evt_tag_int("raw_stream_pos", state->raw_stream_pos),
                     NULL);
        }
      goto error;
    }
  if (state->raw_buffer_size)
    {
      gssize rc;
      guchar *raw_buffer;

      if (!self->super.encoding)
        {
          /* no conversion, we read directly into our buffer */
          if (state->raw_buffer_size > state->buffer_size)
            {
              msg_notice("Invalid LogProtoBufferedServerState.raw_buffer_size, larger than buffer_size and no encoding is set, restarting from the beginning",
                         evt_tag_str("state", persist_name),
                         evt_tag_int("raw_buffer_size", state->raw_buffer_size),
                         evt_tag_int("buffer_size", state->buffer_size),
                         evt_tag_int("init_buffer_size", self->init_buffer_size),
                         NULL);
              goto error;
            }
          raw_buffer = self->buffer;
        }
      else
        {
          if (state->raw_buffer_size > self->init_buffer_size)
            {
              msg_notice("Invalid LogProtoBufferedServerState.raw_buffer_size, larger than init_buffer_size, restarting from the beginning",
                         evt_tag_str("state", persist_name),
                         evt_tag_int("raw_buffer_size", state->raw_buffer_size),
                         evt_tag_int("init_buffer_size", self->init_buffer_size),
                         NULL);
              goto error;
            }
          raw_buffer = g_alloca(state->raw_buffer_size);
        }

      rc = log_transport_read(self->super.transport, raw_buffer, state->raw_buffer_size, NULL);
      if (rc != state->raw_buffer_size)
        {
          msg_notice("Error re-reading buffer contents of the file to be continued, restarting from the beginning",
                     evt_tag_str("state", persist_name),
                     NULL);
          goto error;
        }

      state->pending_buffer_end = 0;
      if (self->super.encoding)
        {
          if (!log_proto_buffered_server_convert_from_raw(self, raw_buffer, rc))
            {
              msg_notice("Error re-converting buffer contents of the file to be continued, restarting from the beginning",
                         evt_tag_str("state", persist_name),
                         NULL);
              goto error;
            }
        }
      else
        {
          state->pending_buffer_end += rc;
        }

      if (state->buffer_pos > state->pending_buffer_end ||
          state->buffer_cached_eol > state->pending_buffer_end)
        {
          msg_notice("Converted buffer contents is smaller than the current buffer position, starting from the beginning of the buffer, some lines may be duplicated",
                     evt_tag_str("state", persist_name),
                     NULL);
          state->buffer_pos = state->pending_buffer_pos = state->buffer_cached_eol = 0;
        }
    }
  else
    {
      /* although we do have buffer position information, but the
       * complete contents of the buffer is already processed, instead
       * of reading and then dropping it, position the file after the
       * indicated block */

      state->raw_stream_pos += state->raw_buffer_size;
      ofs = state->raw_stream_pos;
      state->raw_buffer_size = 0;
      state->buffer_pos = state->pending_buffer_end = 0;

      lseek(fd, state->raw_stream_pos, SEEK_SET);
    }
  goto exit;

 error:
  ofs = 0;
  state->buffer_pos = 0;
  state->pending_buffer_end = 0;
  state->buffer_cached_eol = 0;
  state->raw_stream_pos = 0;
  state->raw_buffer_size = 0;
  state->raw_buffer_leftover_size = 0;
  lseek(fd, 0, SEEK_SET);

 exit:
  state->file_inode = st.st_ino;
  state->file_size = st.st_size;
  state->raw_stream_pos = ofs;
  state->pending_buffer_pos = state->buffer_pos;
  state->pending_raw_stream_pos = state->raw_stream_pos;
  state->pending_raw_buffer_size = state->raw_buffer_size;

  state = NULL;
  log_proto_buffered_server_put_state(self);
}

static PersistEntryHandle
log_proto_buffered_server_alloc_state(LogProtoBufferedServer *self, PersistState *persist_state, const gchar *persist_name)
{
  LogProtoBufferedServerState *state;
  PersistEntryHandle handle;

  handle = persist_state_alloc_entry(persist_state, persist_name, sizeof(LogProtoBufferedServerState));
  if (handle)
    {
      state = persist_state_map_entry(persist_state, handle);

      state->version = 0;
      state->big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);

      persist_state_unmap_entry(persist_state, handle);

    }
  return handle;
}

static gboolean
log_proto_buffered_server_convert_state(LogProtoBufferedServer *self, guint8 persist_version, gpointer old_state, gsize old_state_size, LogProtoBufferedServerState *state)
{
  if (persist_version <= 2)
    {
      state->version = 0;
      state->file_inode = 0;
      state->raw_stream_pos = strtoll((gchar *) old_state, NULL, 10);
      state->file_size = 0;

      return TRUE;
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
                    evt_tag_int("version", version));
          goto error_converting_v3;
        }

      if (!serialize_read_uint64(archive, (guint64 *) &cur_pos) ||
          !serialize_read_uint64(archive, (guint64 *) &cur_inode) ||
          !serialize_read_uint64(archive, (guint64 *) &cur_size))
        {
          msg_error("Internal error restoring information about the current file position, restarting from the beginning",
                    NULL);
          goto error_converting_v3;
        }

      if (!serialize_read_uint16(archive, &version) || version != 0)
        {
          msg_error("Internal error, protocol state has incorrect version",
                    evt_tag_int("version", version),
                    NULL);
          goto error_converting_v3;
        }

      if (!serialize_read_cstring(archive, &buffer, &buffer_len))
        {
          msg_error("Internal error, error reading buffer contents",
                    evt_tag_int("version", version),
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

      state->version = 0;
      state->file_inode = cur_inode;
      state->raw_stream_pos = cur_pos;
      state->file_size = cur_size;
      return TRUE;
    error_converting_v3:
      serialize_archive_free(archive);
    }
  return FALSE;
}

gboolean
log_proto_buffered_server_restart_with_state(LogProto *s, PersistState *persist_state, const gchar *persist_name)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;
  guint8 persist_version;
  PersistEntryHandle old_state_handle;
  gpointer old_state;
  gsize old_state_size;
  PersistEntryHandle new_state_handle = 0;
  gpointer new_state = NULL;
  gboolean success;

  self->persist_state = persist_state;
  old_state_handle = persist_state_lookup_entry(persist_state, persist_name, &old_state_size, &persist_version);
  if (!old_state_handle)
    {
      new_state_handle = log_proto_buffered_server_alloc_state(self, persist_state, persist_name);
      if (!new_state_handle)
        goto fallback_non_persistent;
      log_proto_buffered_server_apply_state(self, new_state_handle, persist_name);
      return TRUE;
    }
  if (persist_version < 4)
    {
      new_state_handle = log_proto_buffered_server_alloc_state(self, persist_state, persist_name);
      if (!new_state_handle)
        goto fallback_non_persistent;

      old_state = persist_state_map_entry(persist_state, old_state_handle);
      new_state = persist_state_map_entry(persist_state, new_state_handle);
      success = log_proto_buffered_server_convert_state(self, persist_version, old_state, old_state_size, new_state);
      persist_state_unmap_entry(persist_state, old_state_handle);
      persist_state_unmap_entry(persist_state, new_state_handle);

      /* we're using the newly allocated state structure regardless if
       * conversion succeeded. If the conversion went wrong then
       * new_state is still in its freshly initialized form since the
       * convert function will not touch the state in the error
       * branches.
       */

      log_proto_buffered_server_apply_state(self, new_state_handle, persist_name);
      return success;
    }
  else if (persist_version == 4)
    {
      LogProtoBufferedServerState *state;

      old_state = persist_state_map_entry(persist_state, old_state_handle);
      state = old_state;
      if ((state->big_endian && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
          (!state->big_endian && G_BYTE_ORDER == G_BIG_ENDIAN))
        {

          /* byte order conversion in order to avoid the hassle with
             scattered byte order conversions in the code */

          state->big_endian = !state->big_endian;
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
        }

      if (state->version > 0)
        {
          msg_error("Internal error restoring log reader state, stored data is too new",
                    evt_tag_int("version", state->version));
          goto error;
        }
      persist_state_unmap_entry(persist_state, old_state_handle);
      log_proto_buffered_server_apply_state(self, old_state_handle, persist_name);
      return TRUE;
    }
  else
    {
      msg_error("Internal error restoring log reader state, stored data is too new",
                evt_tag_int("version", persist_version));
      goto error;
    }
  return TRUE;
 fallback_non_persistent:
  new_state = g_new0(LogProtoBufferedServerState, 1);
 error:
  if (!new_state)
    {
      new_state_handle = log_proto_buffered_server_alloc_state(self, persist_state, persist_name);
      if (!new_state_handle)
        goto fallback_non_persistent;
      new_state = persist_state_map_entry(persist_state, new_state_handle);
    }
  if (new_state)
    {
      LogProtoBufferedServerState *state = new_state;

      /* error happened,  restart the file from the beginning */
      state->raw_stream_pos = 0;
      state->file_inode = 0;
      state->file_size = 0;
      if (new_state_handle)
        log_proto_buffered_server_apply_state(self, new_state_handle, persist_name);
      else
        self->state1 = new_state;
    }
  if (new_state_handle)
    {
      persist_state_unmap_entry(persist_state, new_state_handle);
    }
  return FALSE;
}

static gboolean
log_proto_buffered_server_prepare(LogProto *s, gint *fd, GIOCondition *cond)
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
log_proto_buffered_server_fetch(LogProto *s, const guchar **msg, gsize *msg_len, GSockAddr **sa, gboolean *may_read)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;
  gint rc;
  guchar *raw_buffer = NULL;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(self);
  LogProtoStatus result = self->status;

  if (G_UNLIKELY(!self->buffer))
    {
      self->buffer = g_malloc(self->init_buffer_size);
      state->buffer_size = self->init_buffer_size;
    }

  if (sa)
    *sa = NULL;

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
log_proto_buffered_server_queued(LogProto *s)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(self);

  /* NOTE: we modify the current file position _after_ updating
     buffer_pos, since if we crash right here, at least we
     won't lose data on the next restart, but rather we
     duplicate some data */

  state->buffer_pos = state->pending_buffer_pos;
  state->raw_stream_pos = state->pending_raw_stream_pos;
  state->raw_buffer_size = state->pending_raw_buffer_size;
  if (state->pending_buffer_pos == state->pending_buffer_end)
    {
      state->pending_buffer_end = 0;
      state->buffer_pos = state->pending_buffer_pos = 0;
    }
  if (self->super.flags & LPBS_POS_TRACKING)
    {
      if (state->buffer_pos == state->pending_buffer_end)
        {
          state->raw_stream_pos += state->raw_buffer_size;
          state->raw_buffer_size = 0;
        }
    }
  msg_trace("Last message got confirmed",
            evt_tag_int("raw_stream_pos", state->raw_stream_pos),
            evt_tag_int("raw_buffer_len", state->raw_buffer_size),
            evt_tag_int("buffer_pos", state->buffer_pos),
            evt_tag_int("buffer_end", state->pending_buffer_end),
            evt_tag_int("buffer_cached_eol", state->buffer_cached_eol),
            NULL);
  log_proto_buffered_server_put_state(self);
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
}

void
log_proto_buffered_server_init(LogProtoBufferedServer *self, LogTransport *transport, gint max_buffer_size, gint init_buffer_size, guint flags)
{
  self->super.prepare = log_proto_buffered_server_prepare;
  self->super.fetch = log_proto_buffered_server_fetch;
  self->super.queued = log_proto_buffered_server_queued;
  self->super.free_fn = log_proto_buffered_server_free;
  self->super.transport = transport;
  self->super.convert = (GIConv) -1;
  self->super.restart_with_state = log_proto_buffered_server_restart_with_state;
  self->read_data = log_proto_buffered_server_read_data;

  self->super.flags = flags;

  self->init_buffer_size = init_buffer_size;
  self->max_buffer_size = max_buffer_size;
}

struct _LogProtoTextServer
{
  LogProtoBufferedServer super;
  GIConv reverse_convert;
  gchar *reverse_buffer;
  gsize reverse_buffer_len;
  gint convert_scale;
};

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
log_proto_text_server_is_preemptable(LogProtoTextServer *self)
{
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(&self->super);
  gboolean preemptable;

  preemptable = (state->buffer_cached_eol == 0);
  log_proto_buffered_server_put_state(&self->super);
  return preemptable;
}

static gboolean
log_proto_text_server_prepare(LogProto *s, gint *fd, GIOCondition *cond)
{
  LogProtoTextServer *self = (LogProtoTextServer *) s;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(&self->super);
  gboolean avail;

  if (log_proto_buffered_server_prepare(s, fd, cond))
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
const guchar *
find_eom(const guchar *s, gsize n)
{
  const guchar *char_ptr;
  const gulong *longword_ptr;
  gulong longword, magic_bits, charmask;
  gchar c;

  c = '\n';

  /* align input to long boundary */
  for (char_ptr = s; n > 0 && ((gulong) char_ptr & (sizeof(longword) - 1)) != 0; ++char_ptr, n--)
    {
      if (*char_ptr == c || *char_ptr == '\0')
        return char_ptr;
    }

  longword_ptr = (gulong *) char_ptr;

#if GLIB_SIZEOF_LONG == 8
  magic_bits = 0x7efefefefefefeffL;
#elif GLIB_SIZEOF_LONG == 4
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

struct
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
      gint i;

      /* try to speed up raw size calculation by recognizing the most
       * prominent character encodings and in the case the encoding
       * uses fixed size characters set that in self->convert_scale,
       * which in turn will speed up the reversal of the UTF8 buffer
       * size to raw buffer sizes.
       */
      for (i = 0; fixed_encodings[i].prefix; i++)
        {
          if (strncasecmp(self->super.super.encoding, fixed_encodings[i].prefix, strlen(fixed_encodings[i].prefix) == 0))
            {
              self->convert_scale = fixed_encodings[i].scale;
              break;
            }
        }
      if (!fixed_encodings[i].prefix)
        {
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
                NULL);
      return 0;
    }
  else
    {
      return self->reverse_buffer_len - avail_out;
    }
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

  if (flush_the_rest)
    {
      /*
       * we are set to packet terminating mode or the connection is to
       * be teared down and we have partial data in our buffer.
       */
      *msg = buffer_start;
      *msg_len = buffer_bytes;
      state->pending_buffer_pos = state->pending_buffer_end;
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
      eol = find_eom(buffer_start, buffer_bytes);
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
      state->pending_buffer_pos = eol + 1 - self->super.buffer;

      if (state->pending_buffer_end != state->pending_buffer_pos)
        {
          const guchar *eom;
          /* store the end of the next line, it indicates whether we need
           * to read further data, or the buffer already contains a
           * complete line */
          eom = find_eom(self->super.buffer + state->pending_buffer_pos, state->pending_buffer_end - state->pending_buffer_pos);
          if (eom)
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

void
log_proto_text_server_free(LogProtoTextServer *self)
{
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
  self->reverse_convert = (GIConv) -1;
}

LogProto *
log_proto_text_server_new(LogTransport *transport, gint max_msg_size, guint flags)
{
  LogProtoTextServer *self = g_new0(LogProtoTextServer, 1);

  log_proto_text_server_init(self, transport, max_msg_size, flags);
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

static LogProtoStatus
log_proto_framed_client_post(LogProto *s, guchar *msg, gsize msg_len, gboolean *consumed)
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
      rc = log_proto_text_client_post(s, msg, msg_len, consumed);
      
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
log_proto_framed_server_prepare(LogProto *s, gint *fd, GIOCondition *cond)
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
log_proto_framed_server_fetch(LogProto *s, const guchar **msg, gsize *msg_len, GSockAddr **sa, gboolean *may_read)
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

