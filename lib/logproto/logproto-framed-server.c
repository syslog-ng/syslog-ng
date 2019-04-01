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
#include "logproto-framed-server.h"
#include "messages.h"

#include <errno.h>
#include <ctype.h>
#include <string.h>

#define LPFSS_FRAME_READ    0
#define LPFSS_MESSAGE_READ  1
#define LPFSS_TRIM_MESSAGE  2
#define LPFSS_CONSUME_TRIMMED 3

typedef struct _LogProtoFramedServer
{
  LogProtoServer super;
  gint state;

  guchar *buffer;
  guint32 buffer_size, buffer_pos, buffer_end;
  guint32 frame_len;
  gboolean half_message_in_buffer;
} LogProtoFramedServer;

static LogProtoPrepareAction
log_proto_framed_server_prepare(LogProtoServer *s, GIOCondition *cond, gint *timeout G_GNUC_UNUSED)
{
  LogProtoFramedServer *self = (LogProtoFramedServer *) s;

  *cond = self->super.transport->cond;

  /* there is a half message in our buffer so try to wait */
  if (!self->half_message_in_buffer)
    {
      if (self->buffer_pos != self->buffer_end)
        {
          /* we have a full message in our buffer so parse it without reading new data from the transport layer */
          return LPPA_FORCE_SCHEDULE_FETCH;
        }
    }

  /* if there's no pending I/O in the transport layer, then we want to do a read */
  if (*cond == 0)
    *cond = G_IO_IN;

  return LPPA_POLL_IO;
}

static LogProtoStatus
log_proto_framed_server_fetch_data(LogProtoFramedServer *self, gboolean *may_read, LogTransportAuxData *aux)
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

  rc = log_transport_read(self->super.transport, &self->buffer[self->buffer_end], self->buffer_size - self->buffer_end,
                          aux);

  if (rc < 0)
    {
      if (errno != EAGAIN)
        {
          msg_error("Error reading RFC6587 style framed data",
                    evt_tag_int("fd", self->super.transport->fd),
                    evt_tag_error("error"));
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
      msg_trace("EOF occurred while reading",
                evt_tag_int(EVT_TAG_FD, self->super.transport->fd));
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
                    evt_tag_printf("header", "%.*s", (gint) (i - self->buffer_pos), &self->buffer[self->buffer_pos]));
          return FALSE;
        }
    }
  /* couldn't extract frame header, no error but need more data */
  return TRUE;
}

static LogProtoStatus
log_proto_framed_server_fetch(LogProtoServer *s, const guchar **msg, gsize *msg_len, gboolean *may_read,
                              LogTransportAuxData *aux, Bookmark *bookmark)
{
  LogProtoFramedServer *self = (LogProtoFramedServer *) s;
  LogProtoStatus status;
  gboolean need_more_data, buffer_was_full;

  if (G_UNLIKELY(!self->buffer))
    {
      self->buffer_size = self->super.options->init_buffer_size;
      self->buffer = g_malloc(self->buffer_size);
    }

  switch (self->state)
    {
    case LPFSS_FRAME_READ:
frame_read:
      if (!log_proto_framed_server_extract_frame_length(self, &need_more_data))
        {
          /* invalid frame header */
          log_transport_aux_data_reinit(aux);
          return LPS_ERROR;
        }
      if (need_more_data)
        {
          status = log_proto_framed_server_fetch_data(self, may_read, aux);
          if (status != LPS_SUCCESS)
            {
              log_transport_aux_data_reinit(aux);
              return status;
            }

          if (!log_proto_framed_server_extract_frame_length(self, &need_more_data))
            {
              /* invalid frame header */
              log_transport_aux_data_reinit(aux);
              return LPS_ERROR;
            }
        }

      if (!need_more_data)
        {
          self->state = LPFSS_MESSAGE_READ;
          if (self->frame_len > self->super.options->max_msg_size)
            {
              if (self->super.options->trim_large_messages)
                {
                  msg_debug("Incoming frame larger than log_msg_size(), need to trim.",
                            evt_tag_int("log_msg_size", self->super.options->max_msg_size),
                            evt_tag_int("frame_length", self->frame_len));
                  self->state = LPFSS_TRIM_MESSAGE;
                }
              else
                {
                  msg_error("Incoming frame larger than log_msg_size()",
                            evt_tag_int("log_msg_size", self->super.options->max_msg_size),
                            evt_tag_int("frame_length", self->frame_len));
                  log_transport_aux_data_reinit(aux);
                  return LPS_ERROR;
                }
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
          if (self->state == LPFSS_TRIM_MESSAGE)
            goto trim_message;
          else
            goto read_message;
        }
      break;

    case LPFSS_TRIM_MESSAGE:

trim_message:
      if (self->buffer_end == self->buffer_size)
        {
          /* The buffer is full */
          *msg = &self->buffer[self->buffer_pos];
          *msg_len = self->buffer_end - self->buffer_pos;
          self->frame_len -= *msg_len;
          self->state = LPFSS_CONSUME_TRIMMED;
          self->half_message_in_buffer = TRUE;
          self->buffer_pos = self->buffer_end = 0;
          return LPS_SUCCESS;
        }

      status = log_proto_framed_server_fetch_data(self, may_read, aux);
      if (status != LPS_SUCCESS)
        {
          log_transport_aux_data_reinit(aux);
          return status;
        }

      if (self->buffer_end == self->buffer_size)
        {
          /* The buffer is full */
          *msg = &self->buffer[self->buffer_pos];
          *msg_len = self->buffer_end - self->buffer_pos;
          self->frame_len -= *msg_len;
          self->state = LPFSS_CONSUME_TRIMMED;
          self->half_message_in_buffer = TRUE;
          self->buffer_pos = self->buffer_end = 0;
          return LPS_SUCCESS;
        }
      break;

    case LPFSS_CONSUME_TRIMMED:
      /* Since trimming requires a full (buffer sized) message, the consuming
       * always starts at the beginning of the buffer, with a new read. */
consume_trimmed:
      self->half_message_in_buffer = FALSE;
      status = log_proto_framed_server_fetch_data(self, may_read, aux);
      if (status != LPS_SUCCESS)
        {
          log_transport_aux_data_reinit(aux);
          return status;
        }

      // Fetch data can not report EAGAIN, this means no data in the buffer.
      if (self->buffer_pos == self->buffer_end)
        return LPS_SUCCESS;

      // We have some data, but it is not enough.
      if (self->buffer_end < self->frame_len)
        {
          self->frame_len -= self->buffer_end;
          buffer_was_full = self->buffer_end == self->buffer_size;
          self->buffer_end = 0;

          if (buffer_was_full)
            goto consume_trimmed;
          return LPS_SUCCESS;
        }

      // We have enough data to finish consuming the message.
      self->buffer_pos += self->frame_len;
      self->state = LPFSS_FRAME_READ;

      // Prepare the buffer for the new message.

      // No more data in the buffer
      if (self->buffer_pos == self->buffer_end)
        {
          buffer_was_full = self->buffer_end == self->buffer_size;
          self->buffer_pos = self->buffer_end = 0;
          if (buffer_was_full)
            goto frame_read;
          return LPS_SUCCESS;
        }

      /* We have data in the buffer. Make sure there is enough room for at least
       * one frame header. The rest of it will be solved by frame_read */
      if ((self->buffer_size - self->buffer_pos) < 10)
        {
          memmove(self->buffer, &self->buffer[self->buffer_pos], self->buffer_end - self->buffer_pos);
          self->buffer_end = self->buffer_end - self->buffer_pos;
          self->buffer_pos = 0;
        }
      goto frame_read;
      break;

    case LPFSS_MESSAGE_READ:

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

      status = log_proto_framed_server_fetch_data(self, may_read, aux);
      if (status != LPS_SUCCESS)
        {
          log_transport_aux_data_reinit(aux);
          return status;
        }

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

      break;
    default:
      break;
    }
  return LPS_SUCCESS;
}

static void
log_proto_framed_server_free(LogProtoServer *s)
{
  LogProtoFramedServer *self = (LogProtoFramedServer *) s;
  g_free(self->buffer);

  log_proto_server_free_method(s);
}

LogProtoServer *
log_proto_framed_server_new(LogTransport *transport, const LogProtoServerOptions *options)
{
  LogProtoFramedServer *self = g_new0(LogProtoFramedServer, 1);

  log_proto_server_init(&self->super, transport, options);
  self->super.prepare = log_proto_framed_server_prepare;
  self->super.fetch = log_proto_framed_server_fetch;
  self->super.free_fn = log_proto_framed_server_free;
  self->half_message_in_buffer = FALSE;
  return &self->super;
}
