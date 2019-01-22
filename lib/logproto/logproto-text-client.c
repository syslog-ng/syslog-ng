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
#include "logproto-text-client.h"
#include "messages.h"

#include <errno.h>

static gboolean
log_proto_text_client_prepare(LogProtoClient *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  LogProtoTextClient *self = (LogProtoTextClient *) s;

  *fd = self->super.transport->fd;
  *cond = self->super.transport->cond;

  /* if there's no pending I/O in the transport layer, then we want to do a write */
  if (*cond == 0)
    *cond = G_IO_OUT;
  return self->partial != NULL;
}

static LogProtoStatus
log_proto_text_client_drop_input(LogProtoClient *s)
{
  LogProtoTextClient *self = (LogProtoTextClient *) s;
  guchar buf[1024];
  gint rc = -1;

  do
    {
      rc = log_transport_read(self->super.transport, buf, sizeof(buf), NULL);
    }
  while (rc > 0);

  if (rc == -1 && errno != EAGAIN)
    {
      msg_error("Error reading data", evt_tag_int("fd", self->super.transport->fd), evt_tag_error("error"));
      return LPS_ERROR;
    }
  else if (rc == 0)
    {
      msg_error("EOF occurred while idle", evt_tag_int("fd", log_proto_client_get_fd(&self->super)));
      return LPS_ERROR;
    }

  return LPS_SUCCESS;
}

static LogProtoStatus
log_proto_text_client_flush(LogProtoClient *s)
{
  LogProtoTextClient *self = (LogProtoTextClient *) s;
  gint rc;

  if (!self->partial)
    {
      return LPS_SUCCESS;
    }

  /* attempt to flush previously buffered data */
  gint len = self->partial_len - self->partial_pos;

  rc = log_transport_write(self->super.transport, &self->partial[self->partial_pos], len);
  if (rc < 0)
    {
      if (errno != EAGAIN && errno != EINTR)
        {
          msg_error("I/O error occurred while writing",
                    evt_tag_int("fd", self->super.transport->fd),
                    evt_tag_error(EVT_TAG_OSERROR));
          return LPS_ERROR;
        }
      return LPS_SUCCESS;
    }

  if (rc != len)
    {
      self->partial_pos += rc;
      return LPS_PARTIAL;
    }

  if (self->partial_free)
    self->partial_free(self->partial);
  self->partial = NULL;
  if (self->next_state >= 0)
    {
      self->state = self->next_state;
      self->next_state = -1;
    }

  log_proto_client_msg_ack(&self->super, 1);

  /* NOTE: we return here to give a chance to the framed protocol to send the frame header. */
  return LPS_SUCCESS;
}

LogProtoStatus
log_proto_text_client_submit_write(LogProtoClient *s, guchar *msg, gsize msg_len, GDestroyNotify msg_free,
                                   gint next_state)
{
  LogProtoTextClient *self = (LogProtoTextClient *) s;

  g_assert(self->partial == NULL);
  self->partial = msg;
  self->partial_len = msg_len;
  self->partial_pos = 0;
  self->partial_free = msg_free;
  self->next_state = next_state;
  return log_proto_text_client_flush(s);
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
log_proto_text_client_post(LogProtoClient *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed)
{
  LogProtoTextClient *self = (LogProtoTextClient *) s;

  /* try to flush already buffered data */
  *consumed = FALSE;
  const LogProtoStatus status = log_proto_text_client_flush(s);
  if (status == LPS_ERROR)
    {
      /* log_proto_flush() already logs in the case of an error */
      return status;
    }

  if (self->partial || LPS_PARTIAL == status)
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
      return LPS_PARTIAL;
    }

  *consumed = TRUE;
  return log_proto_text_client_submit_write(s, msg, msg_len, (GDestroyNotify) g_free, -1);
}

void
log_proto_text_client_free(LogProtoClient *s)
{
  LogProtoTextClient *self = (LogProtoTextClient *)s;
  if (self->partial_free)
    self->partial_free(self->partial);
  self->partial = NULL;
  log_proto_client_free_method(s);
};

void
log_proto_text_client_init(LogProtoTextClient *self, LogTransport *transport, const LogProtoClientOptions *options)
{
  log_proto_client_init(&self->super, transport, options);
  self->super.prepare = log_proto_text_client_prepare;
  self->super.flush = log_proto_text_client_flush;
  if (options->drop_input)
    self->super.process_in = log_proto_text_client_drop_input;
  self->super.post = log_proto_text_client_post;
  self->super.free_fn = log_proto_text_client_free;
  self->super.transport = transport;
  self->next_state = -1;
}

LogProtoClient *
log_proto_text_client_new(LogTransport *transport, const LogProtoClientOptions *options)
{
  LogProtoTextClient *self = g_new0(LogProtoTextClient, 1);

  log_proto_text_client_init(self, transport, options);
  return &self->super;
}
