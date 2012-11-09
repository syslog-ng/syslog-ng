/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
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
gint
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


/**
 * This function is called in cases when several files are continously
 * polled for changes.  Whenever the caller would like to switch to another
 * file, it will call this function to check whether it should be allowed to do so.
 *
 * This function returns true if the current state of this LogProtoServer would
 * allow preemption, e.g.  the contents of the current buffer can be
 * discarded.
 **/
static gboolean
log_proto_text_server_is_preemptable(LogProtoServer *s)
{
  LogProtoTextServer *self = (LogProtoTextServer *) s;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(&self->super);
  gboolean preemptable;

  preemptable = (state->buffer_cached_eol == 0);
  log_proto_buffered_server_put_state(&self->super);
  return preemptable;
}

static gboolean
log_proto_text_server_prepare(LogProtoServer *s, gint *fd, GIOCondition *cond)
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
      self->convert_scale = log_proto_get_char_size_for_fixed_encoding(self->super.super.options->encoding);
      if (self->convert_scale == 0)
        {
          /* this encoding is not known, do the conversion for real :( */
          self->reverse_convert = g_iconv_open(self->super.super.options->encoding, "utf-8");
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

      if (G_UNLIKELY(self->super.pos_tracking))
        {
          /* NOTE: we modify the current file position _after_ updating
             buffer_pos, since if we crash right here, at least we
             won't lose data on the next restart, but rather we
             duplicate some data */

          if (self->super.super.options->encoding)
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

static void
log_proto_text_server_free(LogProtoServer *s)
{
  LogProtoTextServer *self = (LogProtoTextServer *) s;
  if (self->reverse_convert != (GIConv) -1)
    g_iconv_close(self->reverse_convert);

  g_free(self->reverse_buffer);
  log_proto_buffered_server_free_method(&self->super.super);
}

void
log_proto_text_server_init(LogProtoTextServer *self, LogTransport *transport, const LogProtoServerOptions *options)
{
  log_proto_buffered_server_init(&self->super, transport, options);
  self->super.super.is_preemptable = log_proto_text_server_is_preemptable;
  self->super.super.prepare = log_proto_text_server_prepare;
  self->super.super.free_fn = log_proto_text_server_free;
  self->super.fetch_from_buf = log_proto_text_server_fetch_from_buf;
  self->super.stream_based = TRUE;
  self->reverse_convert = (GIConv) -1;
}

LogProtoServer *
log_proto_text_server_new(LogTransport *transport, const LogProtoServerOptions *options)
{
  LogProtoTextServer *self = g_new0(LogProtoTextServer, 1);

  log_proto_text_server_init(self, transport, options);
  return &self->super.super;
}
