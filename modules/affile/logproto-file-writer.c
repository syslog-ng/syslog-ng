/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 1998-2012 Balázs Scheidler
 * Copyright (c) 2024 László Várady
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "logproto-file-writer.h"
#include "messages.h"

#include <string.h>
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

typedef struct _LogProtoFileWriter
{
  LogProtoClient super;
  guchar *partial;
  gsize partial_len, partial_pos;
  gint partial_messages;
  gint buf_size;
  gint buf_count;
  gint fd;
  gsize sum_len;
  gboolean fsync;
  struct iovec buffer[0];
} LogProtoFileWriter;

static inline gboolean
_flush_partial(LogProtoFileWriter *self, LogProtoStatus *status)
{
  LogTransport *transport = log_transport_stack_get_active(&self->super.transport_stack);
  /* there is still some data from the previous file writing process */

  gint len = self->partial_len - self->partial_pos;
  gssize rc = log_transport_write(transport, self->partial + self->partial_pos, len);

  if (rc > 0 && self->fsync)
    fsync(self->fd);

  if (rc < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
        {
          *status = LPS_SUCCESS;
          return FALSE;
        }

      log_proto_client_msg_rewind(&self->super);
      msg_error("I/O error occurred while writing",
                evt_tag_int("fd", transport->fd),
                evt_tag_error(EVT_TAG_OSERROR));

      *status = LPS_ERROR;
      return FALSE;
    }

  if (rc != len)
    {
      self->partial_pos += rc;
      *status = LPS_PARTIAL;
      return FALSE;
    }

  log_proto_client_msg_ack(&self->super, self->partial_messages);
  g_free(self->partial);
  self->partial = NULL;
  self->partial_messages = 0;
  return TRUE;
}

static inline void
_process_partial_write(LogProtoFileWriter *self, gsize written)
{
  /* partial success: not everything has been written out */

  /* look for the first chunk that has been cut */
  gsize sum = self->buffer[0].iov_len; /* sum is the cumulated length of the already processed items */
  gint i = 0;
  while (written > sum)
    sum += self->buffer[++i].iov_len;

  gsize first_non_written_msg_chunk_len = sum - written;
  self->partial_len = first_non_written_msg_chunk_len;
  gint first_non_written_chunk_index = i;
  ++i;

  /* add the lengths of the following messages */
  while (i < self->buf_count)
    self->partial_len += self->buffer[i++].iov_len;

  /* allocate and copy the remaining data */
  self->partial = (guchar *)g_malloc(self->partial_len);
  gsize ofs = first_non_written_msg_chunk_len;
  gsize pos = self->buffer[first_non_written_chunk_index].iov_len - ofs;
  memcpy(self->partial, (guchar *) self->buffer[first_non_written_chunk_index].iov_base + pos, ofs);
  i = first_non_written_chunk_index + 1;
  while (i < self->buf_count)
    {
      memcpy(self->partial + ofs, self->buffer[i].iov_base, self->buffer[i].iov_len);
      ofs += self->buffer[i].iov_len;
      ++i;
    }

  self->partial_pos = 0;
  self->partial_messages = self->buf_count - first_non_written_chunk_index;

  log_proto_client_msg_ack(&self->super, self->buf_count - self->partial_messages);
}

/*
 * log_proto_file_writer_flush:
 *
 * this function flushes the file output buffer
 * it is called either form log_proto_file_writer_post (normal mode: the buffer is full)
 * or from log_proto_flush (foced flush: flush time, exit, etc)
 *
 */
static LogProtoStatus
log_proto_file_writer_flush(LogProtoClient *s)
{
  LogProtoFileWriter *self = (LogProtoFileWriter *)s;
  LogTransport *transport = log_transport_stack_get_active(&self->super.transport_stack);

  if (self->partial)
    {
      LogProtoStatus partial_flush_status;
      if (!_flush_partial(self, &partial_flush_status))
        return partial_flush_status;
    }

  /* we might be called from log_writer_deinit() without having a buffer at all */
  if (self->buf_count == 0)
    return LPS_SUCCESS;

  gssize rc = log_transport_writev(transport, self->buffer, self->buf_count);

  if (rc > 0 && self->fsync)
    fsync(transport->fd);

  if (rc < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
        return LPS_SUCCESS;

      log_proto_client_msg_rewind(&self->super);
      msg_error("I/O error occurred while writing",
                evt_tag_int("fd", transport->fd),
                evt_tag_error(EVT_TAG_OSERROR));
      return LPS_ERROR;
    }

  if (rc != self->sum_len)
    _process_partial_write(self, rc);
  else
    log_proto_client_msg_ack(&self->super, self->buf_count);

  /* free the previous message strings (the remaining part has been copied to the partial buffer) */
  for (gint i = 0; i < self->buf_count; ++i)
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
log_proto_file_writer_post(LogProtoClient *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed)
{
  LogProtoFileWriter *self = (LogProtoFileWriter *)s;
  LogProtoStatus result;

  *consumed = FALSE;
  if (self->buf_count >= self->buf_size || self->partial)
    {
      result = log_proto_file_writer_flush(s);
      if (result != LPS_SUCCESS || self->buf_count >= self->buf_size || self->partial)
        {
          /* don't consume a new message if flush failed OR if we couldn't
           * progress in flush() to empty the outgoing buffer (either
           * buffers or partial)
           */
          return result;
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
}

static gboolean
log_proto_file_writer_prepare(LogProtoClient *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  LogProtoFileWriter *self = (LogProtoFileWriter *) s;
  LogTransport *transport = log_transport_stack_get_active(&self->super.transport_stack);

  *fd = transport->fd;
  *cond = transport->cond;

  /* if there's no pending I/O in the transport layer, then we want to do a write */
  if (*cond == 0)
    *cond = G_IO_OUT;
  const gboolean pending_write = self->buf_count > 0 || self->partial;

  if (!pending_write && s->options->timeout > 0)
    *timeout = s->options->timeout;

  return pending_write;
}

LogProtoClient *
log_proto_file_writer_new(LogTransport *transport, const LogProtoClientOptions *options, gint flush_lines, gint fsync_)
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
  LogProtoFileWriter *self = (LogProtoFileWriter *)g_malloc0(sizeof(LogProtoFileWriter) + sizeof(
      struct iovec)*flush_lines);

  log_proto_client_init(&self->super, transport, options);
  self->buf_size = flush_lines;
  self->fsync = fsync_;
  self->super.prepare = log_proto_file_writer_prepare;
  self->super.post = log_proto_file_writer_post;
  self->super.flush = log_proto_file_writer_flush;
  return &self->super;
}
