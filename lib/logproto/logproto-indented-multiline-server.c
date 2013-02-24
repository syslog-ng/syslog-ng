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
 */

#include "logproto-indented-multiline-server.h"
#include "messages.h"

static inline gboolean
log_proto_indented_multiline_is_continuation(guchar first_char)
{
  return (first_char == ' ' || first_char == '\t');
}

static gboolean
log_proto_indented_multiline_extract_message(LogProtoTextServer *s,
                                             const guchar **msg,
                                             gsize *msg_len)
{
  LogProtoIMultiLineServer *self = (LogProtoIMultiLineServer *) s;
  /* NOTES:
   *
   *    msg
   *
   *      points to the beginning of the line _repeatedly_, e.g. as
   *      long as we return the we couldn't extract a message.
   *
   *    msg_len
   *
   *      This is getting longer and longer as lines get accumulated
   *      in the message. To find out where the last EOL was, this
   *      function must keep track of its previous invocations.
   *
   *    self->last_eol_pos
   *
   *      This is where we save the location of the last EOL, it is
   *      relative to the beginning of the current message.
   *
   */

  /* let's check if the current line is a continuation line or not */
  if (self->super.accumulated_msg_len > 0 && *msg_len > self->super.accumulated_msg_len + 1)
    {
      guchar first_character_of_the_current_line = (*msg)[self->super.accumulated_msg_len + 1];

      if (log_proto_indented_multiline_is_continuation(first_character_of_the_current_line))
        {
          return LPT_CONSUME_LINE | LPT_WAITING;
        }
      else
        {
          return LPT_REWIND_LINE | LPT_EXTRACTED;
        }
    }

  return LPT_CONSUME_LINE | LPT_WAITING;
}

void
log_proto_indented_multiline_server_init(LogProtoIMultiLineServer *self,
                                         LogTransport *transport,
                                         const LogProtoServerOptions *options)
{
  log_proto_text_server_init(&self->super, transport, options);
  self->super.extract_message = log_proto_indented_multiline_extract_message;
}

LogProtoServer *
log_proto_indented_multiline_server_new(LogTransport *transport, const LogProtoServerOptions *options)
{
  LogProtoIMultiLineServer *self = g_new0(LogProtoIMultiLineServer, 1);

  log_proto_indented_multiline_server_init(self, transport, options);
  return &self->super.super.super;
}
