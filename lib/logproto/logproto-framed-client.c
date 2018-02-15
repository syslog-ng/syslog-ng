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
#include "logproto-framed-client.h"
#include "logproto-text-client.h"
#include "messages.h"

#define LPFCS_FRAME_SEND    0
#define LPFCS_MESSAGE_SEND  1

typedef struct _LogProtoFramedClient
{
  LogProtoTextClient super;
  guchar frame_hdr_buf[9];
} LogProtoFramedClient;

static LogProtoStatus
log_proto_framed_client_post(LogProtoClient *s, LogMessage *logmsg, guchar *msg, gsize msg_len, gboolean *consumed)
{
  LogProtoFramedClient *self = (LogProtoFramedClient *) s;
  gint frame_hdr_len;
  gint rc;

  if (msg_len > 9999999)
    {
      static const guchar *warn_msg;

      if (warn_msg != msg)
        {
          msg_warning("Error, message length too large for framed protocol, truncated",
                      evt_tag_int("length", msg_len));
          warn_msg = msg;
        }
      msg_len = 9999999;
    }

  rc = LPS_SUCCESS;
  while (rc == LPS_SUCCESS && !(*consumed) && self->super.partial == NULL)
    {
      switch (self->super.state)
        {
        case LPFCS_FRAME_SEND:
          frame_hdr_len = g_snprintf((gchar *) self->frame_hdr_buf, sizeof(self->frame_hdr_buf), "%" G_GSIZE_FORMAT" ", msg_len);
          rc = log_proto_text_client_submit_write(s, self->frame_hdr_buf, frame_hdr_len, NULL, LPFCS_MESSAGE_SEND);
          break;
        case LPFCS_MESSAGE_SEND:
          *consumed = TRUE;
          rc = log_proto_text_client_submit_write(s, msg, msg_len, (GDestroyNotify) g_free, LPFCS_FRAME_SEND);
          break;
        default:
          g_assert_not_reached();
        }
    }

  return rc;
}

LogProtoClient *
log_proto_framed_client_new(LogTransport *transport, const LogProtoClientOptions *options)
{
  LogProtoFramedClient *self = g_new0(LogProtoFramedClient, 1);

  log_proto_text_client_init(&self->super, transport, options);
  self->super.super.post = log_proto_framed_client_post;
  self->super.state = LPFCS_FRAME_SEND;
  return &self->super.super;
}
