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
#include "logproto-text-server.h"
#include "messages.h"

#include <string.h>

LogProtoPrepareAction
log_proto_text_server_prepare_method(LogProtoServer *s, GIOCondition *cond, gint *timeout)
{
  LogProtoTextServer *self = (LogProtoTextServer *) s;
  gboolean avail;

  LogProtoPrepareAction action = log_proto_buffered_server_prepare(s, cond, timeout);
  if (action != LPPA_POLL_IO)
    return action;

  avail = (self->cached_eol_pos != 0);
  return avail ? LPPA_FORCE_SCHEDULE_FETCH : LPPA_POLL_IO;
}

static gint
log_proto_text_server_accumulate_line(LogProtoTextServer *self, const guchar *msg, gsize msg_len,
                                      gssize consumed_len)
{
  if (self->multi_line)
    {
      const guchar *segment = &msg[consumed_len + 1];
      gsize segment_len = msg_len - (segment - msg);

      return multi_line_logic_accumulate_line(self->multi_line,
                                              msg, consumed_len < 0 ? 0 : consumed_len,
                                              segment, segment_len);
    }

  return MLL_CONSUME_SEGMENT | MLL_EXTRACTED;
}

static gboolean
log_proto_text_server_try_extract(LogProtoTextServer *self, LogProtoBufferedServerState *state,
                                  const guchar *buffer_start, gsize buffer_bytes, const guchar *eol, const guchar **msg, gsize *msg_len)
{
  gint verdict;
  guint32 next_line_pos;
  guint32 next_eol_pos = 0;

  next_line_pos = eol + 1 - self->super.buffer;
  if (state->pending_buffer_end != next_line_pos)
    {
      const guchar *eom;

      /* we have some more data in the buffer, check if we have a
       * subsequent EOL there.  It indicates whether we need to
       * read further data, or the buffer already contains a
       * complete line */

      eom = self->find_eom(self->super.buffer + next_line_pos, state->pending_buffer_end - next_line_pos);
      if (eom)
        next_eol_pos = eom - self->super.buffer;
    }

  *msg_len = eol - buffer_start;
  *msg = buffer_start;

  verdict = log_proto_text_server_accumulate_line(self, *msg, *msg_len, self->consumed_len);
  if (verdict & MLL_EXTRACTED)
    {
      if (verdict & MLL_CONSUME_SEGMENT)
        {
          gint drop_length = (verdict & MLL_CONSUME_PARTIAL_AMOUNT_MASK) >> MLL_CONSUME_PARTIAL_AMOUNT_SHIFT;

          state->pending_buffer_pos = next_line_pos;
          self->cached_eol_pos = next_eol_pos;
          if (drop_length)
            *msg_len -= drop_length;
        }
      else if (verdict & MLL_REWIND_SEGMENT)
        {
          if (self->consumed_len >= 0)
            *msg_len = self->consumed_len;
          else
            *msg_len = 0;

          state->pending_buffer_pos = (buffer_start + self->consumed_len + 1) - self->super.buffer;
          self->cached_eol_pos = eol - self->super.buffer;
        }
      else
        g_assert_not_reached();
      self->consumed_len = -1;
    }
  else if (verdict & MLL_WAITING)
    {
      *msg = NULL;
      *msg_len = 0;
      if (verdict & MLL_CONSUME_SEGMENT)
        {
          gint drop_length = (verdict & MLL_CONSUME_PARTIAL_AMOUNT_MASK) >> MLL_CONSUME_PARTIAL_AMOUNT_SHIFT;

          /* NOTE: we can't partially consume a line at the middle of a
           * multi-line construct, as that would mean we would have to copy
           * stuff to a separate buffer, which we don't at the moment for
           * performance reasons.
           *
           * No implementation of the MultiLineLogic interface does that and
           * honestly I can't even see a use-case at the moment: we only use
           * MLL_CONSUME_PARTIALLY() to truncate the end of the last line
           * (when "garbage" is found).  Should we ever want to add an
           * implementation that extracts only parts of the incoming lines,
           * even in the middle, we would probably have to check the
           * interface, and validate at init() time that LogProtoTextBuffer
           * is unable to work with something that does that.
           *
           * At the moment we will violently crash if this happens with a
           * SIGABRT.
           */

          g_assert(drop_length == 0);
          self->cached_eol_pos = next_eol_pos;
          self->consumed_len = eol - buffer_start;
        }
      else if (verdict & MLL_REWIND_SEGMENT)
        {
          /* when we are waiting for another line, the current one
           * can't be rewinded, so MLL_REWIND_SEGMENT is not valid */
          g_assert_not_reached();
        }
      else
        {
          g_assert_not_reached();
        }
      return FALSE;
    }
  else
    g_assert_not_reached();
  return TRUE;
}

static gboolean
log_proto_text_server_extract(LogProtoTextServer *self, LogProtoBufferedServerState *state, const guchar *buffer_start,
                              gsize buffer_bytes, const guchar *eol, const guchar **msg, gsize *msg_len)
{
  do
    {
      if (log_proto_text_server_try_extract(self, state, buffer_start, buffer_bytes, eol, msg, msg_len))
        return TRUE;
      eol = self->super.buffer + self->cached_eol_pos;
    }
  while (self->cached_eol_pos > 0);
  return FALSE;
}

static void
log_proto_text_server_remove_trailing_newline(const guchar **msg, gsize *msg_len)
{
  const guchar *msg_start = (*msg);
  const guchar *msg_end = msg_start + (*msg_len);

  /* msg_end points at the newline character. A \r or \0 may precede
   * this which should be removed from the message body */

  while ((msg_end > msg_start) && (msg_end[-1] == '\r' || msg_end[-1] == '\n' || msg_end[-1] == 0))
    msg_end--;
  *msg_len = msg_end - msg_start;
}


static inline void
log_proto_text_server_yield_whole_buffer_as_message(LogProtoTextServer *self, LogProtoBufferedServerState *state,
                                                    const guchar *buffer_start, gsize buffer_bytes, const guchar **msg, gsize *msg_len)
{
  /* no EOL, our buffer is full, no way to move forward, return
   * everything we have in our buffer. */

  *msg = buffer_start;
  *msg_len = buffer_bytes;
  self->consumed_len = -1;
  state->pending_buffer_pos = (*msg) + (*msg_len) - self->super.buffer;
}

static inline const guchar *
log_proto_text_server_locate_next_eol(LogProtoTextServer *self, LogProtoBufferedServerState *state,
                                      const guchar *buffer_start, gsize buffer_bytes)
{
  const guchar *eol;

  if (self->cached_eol_pos)
    {
      /* previous invocation was nice enough to save a cached EOL
       * pointer, no need to look it up again */

      eol = self->super.buffer + self->cached_eol_pos;
      self->cached_eol_pos = 0;
    }
  else
    {
      eol = self->find_eom(buffer_start + self->consumed_len + 1, buffer_bytes - self->consumed_len - 1);
    }
  return eol;
}

static gboolean
log_proto_text_server_message_size_too_large(LogProtoTextServer *self, gsize buffer_bytes)
{
  return buffer_bytes >= self->super.super.options->max_msg_size;
}

static inline gboolean
_fetch_msg_from_buffer(LogProtoTextServer *self, LogProtoBufferedServerState *state,
                       const guchar *buffer_start, gsize buffer_bytes,
                       const guchar **msg, gsize *msg_len)
{
  const guchar *eol = log_proto_text_server_locate_next_eol(self, state, buffer_start, buffer_bytes);

  if (!eol)
    {
      if (log_proto_text_server_message_size_too_large(self, buffer_bytes)
          || log_proto_buffered_server_is_input_closed(&self->super))
        {
          log_proto_text_server_yield_whole_buffer_as_message(self, state, buffer_start, buffer_bytes, msg, msg_len);
          goto success;
        }

      return FALSE;
    }

  if (log_proto_text_server_extract(self, state, buffer_start, buffer_bytes, eol, msg, msg_len))
    goto success;

  if (log_proto_text_server_message_size_too_large(self, buffer_bytes))
    {
      log_proto_text_server_yield_whole_buffer_as_message(self, state, buffer_start, buffer_bytes, msg, msg_len);
      goto success;
    }

  return FALSE;

success:
  log_proto_text_server_remove_trailing_newline(msg, msg_len);
  return TRUE;
}

static gboolean
log_proto_text_server_fetch_from_buffer(LogProtoBufferedServer *s, const guchar *buffer_start, gsize buffer_bytes,
                                        const guchar **msg, gsize *msg_len)
{
  LogProtoTextServer *self = (LogProtoTextServer *) s;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(&self->super);

  gboolean result = _fetch_msg_from_buffer(self, state, buffer_start, buffer_bytes, msg, msg_len);

  log_proto_buffered_server_put_state(&self->super);
  return result;
}

static void
log_proto_text_server_flush(LogProtoBufferedServer *s)
{
  LogProtoTextServer *self = (LogProtoTextServer *) s;
  self->consumed_len = -1;
  self->cached_eol_pos = 0;
}

void
log_proto_text_server_set_multi_line(LogProtoServer *s, MultiLineLogic *multi_line)
{
  LogProtoTextServer *self = (LogProtoTextServer *) s;

  if (self->multi_line)
    multi_line_logic_free(self->multi_line);
  self->multi_line = multi_line;
}

void
log_proto_text_server_free(LogProtoServer *s)
{
  LogProtoTextServer *self = (LogProtoTextServer *) s;
  if (self->multi_line)
    multi_line_logic_free(self->multi_line);
  log_proto_buffered_server_free_method(&self->super.super);
}

void
log_proto_text_server_init(LogProtoTextServer *self, LogTransport *transport, const LogProtoServerOptions *options)
{
  log_proto_buffered_server_init(&self->super, transport, options);
  self->super.super.prepare = log_proto_text_server_prepare_method;
  self->super.super.free_fn = log_proto_text_server_free;
  self->super.fetch_from_buffer = log_proto_text_server_fetch_from_buffer;
  self->super.flush = log_proto_text_server_flush;
  self->find_eom = find_eom;
  self->super.stream_based = TRUE;
  self->consumed_len = -1;
}

LogProtoServer *
log_proto_text_server_new(LogTransport *transport, const LogProtoServerOptions *options)
{
  LogProtoTextServer *self = g_new0(LogProtoTextServer, 1);

  log_proto_text_server_init(self, transport, options);
  return &self->super.super;
}

static const guchar *
_find_nl_as_eom(const guchar *s, gsize n)
{
  return memchr(s, '\n', n);
}

LogProtoServer *
log_proto_text_with_nuls_server_new(LogTransport *transport, const LogProtoServerOptions *options)
{
  LogProtoTextServer *self = g_new0(LogProtoTextServer, 1);

  log_proto_text_server_init(self, transport, options);
  self->find_eom = _find_nl_as_eom;
  return &self->super.super;
}
