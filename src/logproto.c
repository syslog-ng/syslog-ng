/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "logproto.h"
#include "messages.h"
#include "persist-state.h"
#include "compat.h"

#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

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


typedef struct _LogProtoPlainClient
{
  LogProto super;
  guchar *partial;
  gsize partial_len, partial_pos;
} LogProtoPlainClient;

static gboolean
log_proto_plain_client_prepare(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  LogProtoPlainClient *self = (LogProtoPlainClient *) s;
  
  *fd = self->super.transport->fd;
  *cond = self->super.transport->cond;

  /* if there's no pending I/O in the transport layer, then we want to do a write */
  if (*cond == 0)
    *cond = G_IO_OUT;
  return FALSE;
}

/*
 * log_proto_plain_client_post:
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
log_proto_plain_client_post(LogProto *s, guchar *msg, gsize msg_len, gboolean *consumed)
{
  LogProtoPlainClient *self = (LogProtoPlainClient *) s;
  gint rc;

  /* NOTE: the client does not support charset conversion for now */
  g_assert(self->super.convert == (GIConv) -1);
  
  *consumed = FALSE;
  /* attempt to flush previously buffered data */
  if (self->partial)
    {
      gint len = self->partial_len - self->partial_pos;
      
      rc = log_transport_write(self->super.transport, &self->partial[self->partial_pos], len);
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
log_proto_plain_new_client(LogTransport *transport)
{
  LogProtoPlainClient *self = g_new0(LogProtoPlainClient, 1);

  self->super.prepare = log_proto_plain_client_prepare;  
  self->super.post = log_proto_plain_client_post;
  self->super.transport = transport;
  self->super.convert = (GIConv) -1;
  return &self->super;
}

typedef struct _LogProtoPlainServerState
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
} LogProtoPlainServerState;

struct _LogProtoPlainServer
{
  LogProto super;
  LogProtoPlainServerState *state;
  GIConv reverse_convert;
  gchar *reverse_buffer;
  gsize reverse_buffer_len;
  gint convert_scale;
  guchar *buffer;
  gsize padding_size, max_msg_size;
  GSockAddr *prev_saddr;
  LogProtoStatus status;
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
log_proto_plain_server_is_preemptable(LogProtoPlainServer *self)
{ 
  return (self->state->buffer_cached_eol == 0);
}

static gboolean
log_proto_plain_server_prepare(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  LogProtoPlainServer *self = (LogProtoPlainServer *) s;
  
  *fd = self->super.transport->fd;
  *cond = self->super.transport->cond;

  /* if there's no pending I/O in the transport layer, then we want to do a read */
  if (*cond == 0)
    *cond = G_IO_IN;

  return self->state->buffer_cached_eol != 0;
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
log_proto_plain_server_get_raw_size_of_buffer(LogProtoPlainServer *self, const guchar *buffer, gsize buffer_len)
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
          if (strncasecmp(self->super.encoding, fixed_encodings[i].prefix, strlen(fixed_encodings[i].prefix) == 0))
            {
              self->convert_scale = fixed_encodings[i].scale;
              break;
            }
        }
      if (!fixed_encodings[i].prefix)
        {
          self->reverse_convert = g_iconv_open(self->super.encoding, "utf-8");
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

static gboolean
log_proto_plain_server_convert_from_raw(LogProtoPlainServer *self, const guchar *raw_buffer, gsize raw_buffer_len)
{
  /* some data was read */
  gsize avail_in = raw_buffer_len;
  gsize avail_out;
  gchar *out;
  gint  ret = -1;

  do
    {
      avail_out = self->state->buffer_size - self->state->pending_buffer_end;
      out = (gchar *) self->buffer + self->state->pending_buffer_end;

      ret = g_iconv(self->super.convert, (gchar **) &raw_buffer, &avail_in, (gchar **) &out, &avail_out);
      if (ret == (gsize) -1)
        {
          switch (errno)
            {
            case EINVAL:
              /* Incomplete text, do not report an error, rather try to read again */
              self->state->pending_buffer_end = self->state->buffer_size - avail_out;

              if (avail_in > 0)
                {
                  if (avail_in > sizeof(self->state->raw_buffer_leftover))
                    {
                      msg_error("Invalid byte sequence, the remaining raw buffer is larger than the supported leftover size",
                                evt_tag_str("encoding", self->super.encoding),
                                evt_tag_int("avail_in", avail_in),
                                evt_tag_int("leftover_size", sizeof(self->state->raw_buffer_leftover)),
                                NULL);
                      return FALSE;
                    }
                  memcpy(self->state->raw_buffer_leftover, raw_buffer, avail_in);
                  self->state->raw_buffer_leftover_size = avail_in;
                  self->state->raw_buffer_size -= avail_in;
                  msg_trace("Leftover characters remained after conversion, delaying message until another chunk arrives",
                            evt_tag_str("encoding", self->super.encoding),
                            evt_tag_int("avail_in", avail_in),
                            NULL);
                  return TRUE;
                }
              break;
            case E2BIG:
              self->state->pending_buffer_end = self->state->buffer_size - avail_out;
              /* extend the buffer */

              if ((self->state->buffer_size < self->max_msg_size * 6))
                {
                  /* we can only resize our state if we're using our malloced state */

                  self->state->buffer_size *= 2;
                  self->buffer = g_realloc(self->buffer, self->state->buffer_size);

                  /* recalculate the out pointer, and add what we have now */
                  ret = -1;
                }
              else
                {
                  msg_error("Incoming byte stream requires a too large conversion buffer, probably invalid character sequence",
                            evt_tag_str("encoding", self->super.encoding),
                            evt_tag_printf("buffer", "%.*s", (gint) self->state->pending_buffer_end, self->buffer),
                            NULL);
                  return FALSE;
                }
              break;
            case EILSEQ:
            default:
              msg_notice("Invalid byte sequence or other error while converting input, skipping character",
                         evt_tag_str("encoding", self->super.encoding),
                         evt_tag_printf("char", "0x%02x", *(guchar *) raw_buffer),
                         NULL);
              return FALSE;
            }
        }
      else
        {
          self->state->pending_buffer_end = self->state->buffer_size - avail_out;
        }
    }
  while (avail_in > 0);

  return TRUE;
}

/**
 * log_proto_plain_server_fetch_from_buf:
 * @self: LogReader instance
 * @saddr: socket address to be assigned to new messages (consumed!)
 * @flush: whether to flush the input buffer
 * @msg_counter: the number of messages processed in the current poll iteration
 * 
 * Returns TRUE if a message was found in the buffer, FALSE if we need to read again.
 **/
static gboolean
log_proto_plain_server_fetch_from_buf(LogProtoPlainServer *self, const guchar **msg, gsize *msg_len, gboolean flush_the_rest)
{
  const guchar *eol;
  gsize buffer_bytes;
  const guchar *buffer_start;

  buffer_start = self->buffer + self->state->pending_buffer_pos;
  buffer_bytes = self->state->pending_buffer_end - self->state->pending_buffer_pos;

  if (buffer_bytes == 0)
    {
      /* if buffer_bytes is zero bytes, it means that we completely
       * processed our buffer without having a fraction of a line still
       * there.  It is important to reset
       * pending_buffer_pos/pending_buffer_end to zero as the caller assumes
       * that if we return no message from the buffer, then buffer_pos is
       * _zero_.
       */

      if (G_UNLIKELY(self->super.flags & LPPF_POS_TRACKING))
        {
          self->state->pending_raw_stream_pos += self->state->pending_raw_buffer_size;
          self->state->pending_raw_buffer_size = 0;
        }
      self->state->pending_buffer_pos = self->state->pending_buffer_end = 0;
      return FALSE;
    }

  if ((self->super.flags & LPPF_PKTTERM) || flush_the_rest)
    {
      /*
       * we are set to packet terminating mode or the connection is to
       * be teared down and we have partial data in our buffer.
       */
      *msg = buffer_start;
      *msg_len = buffer_bytes;
      self->state->pending_buffer_pos = self->state->pending_buffer_end;
      return TRUE;
    }
  
  if (self->state->buffer_cached_eol)
    {
      /* previous invocation was nice enough to save a cached EOL
       * pointer, no need to look it up again */

      eol = self->buffer + self->state->buffer_cached_eol;
      self->state->buffer_cached_eol = 0;
    }
  else
    {
      eol = find_eom(buffer_start, buffer_bytes);
    }
  if ((!eol && (buffer_bytes == self->state->buffer_size)) || self->padding_size)
    {
      /* our buffer is full and no EOL was found, or we're in HP-UX padded mode */
      *msg_len = (self->padding_size
                  ? (eol ? eol - buffer_start : buffer_bytes)
                  : buffer_bytes);
      self->state->pending_buffer_pos = self->state->pending_buffer_end;
      *msg = buffer_start;
      return TRUE;
    }
  else if (!eol)
    {
      gsize raw_split_size;

      /* buffer is not full, but no EOL is present, move partial line
       * to the beginning of the buffer to make space for new data.
       */

      memmove(self->buffer, buffer_start, buffer_bytes);
      self->state->pending_buffer_pos = 0;
      self->state->pending_buffer_end = buffer_bytes;

      if (G_UNLIKELY(self->super.flags & LPPF_POS_TRACKING))
        {
          /* NOTE: we modify the current file position _after_ updating
             buffer_pos, since if we crash right here, at least we
             won't lose data on the next restart, but rather we
             duplicate some data */

          if (self->super.encoding)
            raw_split_size = log_proto_plain_server_get_raw_size_of_buffer(self, buffer_start, buffer_bytes);
          else
            raw_split_size = buffer_bytes;

          self->state->pending_raw_stream_pos += (gint64) (self->state->pending_raw_buffer_size - raw_split_size);
          self->state->pending_raw_buffer_size = raw_split_size;

          msg_trace("Buffer split",
                    evt_tag_int("raw_split_size", raw_split_size),
                    evt_tag_int("buffer_bytes", buffer_bytes),
                    NULL);
        }
      return FALSE;
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
      self->state->pending_buffer_pos = eol + 1 - self->buffer;
      
      if (self->state->pending_buffer_end != self->state->pending_buffer_pos)
        {
          const guchar *eom;
          /* store the end of the next line, it indicates whether we need
           * to read further data, or the buffer already contains a
           * complete line */
          eom = find_eom(self->buffer + self->state->pending_buffer_pos, self->state->pending_buffer_end - self->state->pending_buffer_pos);
          if (eom)
            self->state->buffer_cached_eol = eom - self->buffer;
          else
            self->state->buffer_cached_eol = 0;
        }
      else
        {
          self->state->pending_buffer_pos = self->state->pending_buffer_end;
        }
      return TRUE;
    }
}

/**
 * Returns: TRUE to indicate success, FALSE otherwise. The returned
 * msg can be NULL even if no failure occurred.
 **/
static LogProtoStatus
log_proto_plain_server_fetch(LogProto *s, const guchar **msg, gsize *msg_len, GSockAddr **sa, gboolean *may_read)
{
  LogProtoPlainServer *self = (LogProtoPlainServer *) s;
  gint rc;
  guchar *raw_buffer = NULL;


  if (sa)
    *sa = NULL;

  if (self->status != LPS_SUCCESS)
    return self->status;

  if (log_proto_plain_server_fetch_from_buf(self, msg, msg_len, FALSE))
    {
      if (sa && self->prev_saddr)
        *sa = g_sockaddr_ref(self->prev_saddr);
      return LPS_SUCCESS;
    }

  /* ok, no more messages in the buffer, read a chunk */
  while (*may_read)
    {
      gint avail;

      if (self->super.flags & LPPF_NOMREAD)
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
          raw_buffer = self->buffer + self->state->pending_buffer_end;
          if (!self->padding_size)
            avail = self->state->buffer_size - self->state->pending_buffer_end;
          else
            {

              /* data is read/processed in "padding" sized chunks,
               * thus iff padding > buffer_size we never fail this
               * assertion */
              g_assert(self->state->buffer_size - self->state->pending_buffer_end >= self->padding_size);
              avail = self->padding_size;
            }
        }
      else
        {
          /* if conversion is needed, we first read into an on-stack
           * buffer, and then convert it into our internal buffer */

          raw_buffer = g_alloca(self->max_msg_size + self->state->raw_buffer_leftover_size);
          memcpy(raw_buffer, self->state->raw_buffer_leftover, self->state->raw_buffer_leftover_size);
          if (!self->padding_size)
            {
              avail = self->max_msg_size;
            }
          else
            {
              g_assert(self->max_msg_size > self->padding_size);
              avail = self->padding_size;
            }
        }

      rc = log_transport_read(self->super.transport, raw_buffer + self->state->raw_buffer_leftover_size, avail, sa);
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
              self->status = LPS_ERROR;
              if (log_proto_plain_server_fetch_from_buf(self, msg, msg_len, TRUE))
                {
                  if (sa && self->prev_saddr)
                    *sa = g_sockaddr_ref(self->prev_saddr);
                  return LPS_SUCCESS;
                }
              return self->status;
            }
        }
      else if (rc == 0)
        {
          if ((self->super.flags & LPPF_IGNORE_EOF) == 0)
            {
              /* EOF read */
              msg_verbose("EOF occurred while reading", 
                          evt_tag_int(EVT_TAG_FD, self->super.transport->fd),
                          NULL);
              if (self->state->raw_buffer_leftover_size > 0)
                {
                  msg_error("EOF read on a channel with leftovers from previous character conversion, dropping input",
                            NULL);
                  self->status = LPS_EOF;
                  return self->status;
                }
              self->status = LPS_EOF;
              if (log_proto_plain_server_fetch_from_buf(self, msg, msg_len, TRUE))
                {
                  if (sa && self->prev_saddr)
                    *sa = g_sockaddr_ref(self->prev_saddr);
                  return LPS_SUCCESS;
                }
              return self->status;
            }
          else
            {
              *msg = NULL;
              *msg_len = 0;
              return LPS_SUCCESS;
            }
        }
      else if (self->padding_size && rc != self->padding_size)
        {
          msg_error("Padding was set, and couldn't read enough bytes",
                    evt_tag_int(EVT_TAG_FD, self->super.transport->fd),
                    evt_tag_int("padding", self->padding_size),
                    evt_tag_int("read", avail),
                    NULL);
          self->status = LPS_ERROR;
          return self->status;
        }
      else
        {
          self->state->pending_raw_buffer_size += rc;
          rc += self->state->raw_buffer_leftover_size;
          self->state->raw_buffer_leftover_size = 0;

          if (self->super.encoding)
            {
              if (!log_proto_plain_server_convert_from_raw(self, raw_buffer, rc))
                {
                  self->status = LPS_ERROR;
                  return self->status;
                }
            }
          else
            {
              self->state->pending_buffer_end += rc;
            }

          if (log_proto_plain_server_fetch_from_buf(self, msg, msg_len, FALSE))
            {
              if (sa && self->prev_saddr)
                *sa = g_sockaddr_ref(self->prev_saddr);
              return LPS_SUCCESS;
            }          
        }
    }
  return LPS_SUCCESS;
}

void
log_proto_plain_server_queued(LogProto *s)
{
  LogProtoPlainServer *self = (LogProtoPlainServer *) s;

  /* NOTE: we modify the current file position _after_ updating
     buffer_pos, since if we crash right here, at least we
     won't lose data on the next restart, but rather we
     duplicate some data */

  self->state->buffer_pos = self->state->pending_buffer_pos;
  self->state->raw_stream_pos = self->state->pending_raw_stream_pos;
  self->state->raw_buffer_size = self->state->pending_raw_buffer_size;
  if (self->state->pending_buffer_pos == self->state->pending_buffer_end)
    {
      self->state->pending_buffer_end = 0;
      self->state->buffer_pos = self->state->pending_buffer_pos = 0;
    }
  if (self->super.flags & LPPF_POS_TRACKING)
    {
      if (self->state->buffer_pos == self->state->pending_buffer_end)
        {
          self->state->raw_stream_pos += self->state->raw_buffer_size;
          self->state->raw_buffer_size = 0;
        }
    }
  msg_trace("Last message got confirmed",
            evt_tag_int("raw_stream_pos", self->state->raw_stream_pos),
            evt_tag_int("raw_buffer_len", self->state->raw_buffer_size),
            evt_tag_int("buffer_pos", self->state->buffer_pos),
            evt_tag_int("buffer_end", self->state->pending_buffer_end),
            evt_tag_int("buffer_cached_eol", self->state->buffer_cached_eol),
            NULL);

}

void
log_proto_plain_server_free(LogProto *s)
{
  LogProtoPlainServer *self = (LogProtoPlainServer *) s;

  g_sockaddr_unref(self->prev_saddr);

  if (self->reverse_convert != (GIConv) -1)
    g_iconv_close(self->reverse_convert);

  g_free(self->reverse_buffer);
  g_free(self->buffer);
  g_free(self->state);
}

void
log_proto_plain_server_init(LogProtoPlainServer *self, LogTransport *transport, gint padding_size, gint max_msg_size, guint flags)
{
  self->super.prepare = log_proto_plain_server_prepare;  
  self->super.fetch = log_proto_plain_server_fetch;
  self->super.queued = log_proto_plain_server_queued;
  self->super.free_fn = log_proto_plain_server_free;
  self->super.transport = transport;
  self->super.convert = (GIConv) -1;
  self->reverse_convert = (GIConv) -1;

  self->super.flags = flags;
  self->padding_size = padding_size;
  self->max_msg_size = max_msg_size;
}

LogProto *
log_proto_plain_new_server(LogTransport *transport, gint padding_size, gint max_msg_size, guint flags)
{
  LogProtoPlainServer *self = g_new0(LogProtoPlainServer, 1);

  log_proto_plain_server_init(self, transport, padding_size, max_msg_size, flags);
  self->state = g_new0(LogProtoPlainServerState, 1);
  self->buffer = g_malloc(max_msg_size);
  self->state->buffer_size = max_msg_size;
  return &self->super;
}

struct _LogProtoFileReader
{
  LogProtoPlainServer super;
  PersistState *persist_state;
  PersistEntryHandle persist_handle;
  gchar *filename;
};

typedef struct _LogProtoFileReaderState
{
  LogProtoPlainServerState super;
  gint64 file_size;
  gint64 file_inode;
} LogProtoFileReaderState;

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
log_proto_file_reader_is_preemptable(LogProtoFileReader *self)
{
  LogProtoFileReaderState *state;
  gboolean result = FALSE;

  state = persist_state_map_entry(self->persist_state, self->persist_handle);
  result = (state->super.buffer_cached_eol == 0);
  persist_state_unmap_entry(self->persist_state, self->persist_handle);
  return result;
}


static void
log_proto_file_reader_apply_state(LogProtoFileReader *self, PersistEntryHandle handle)
{
  struct stat st;
  gint64 ofs = 0;
  LogProtoFileReaderState *state;
  gint fd;

  fd = self->super.super.transport->fd;
  self->persist_handle = handle;

  if (fstat(fd, &st) < 0)
    return;

  state = persist_state_map_entry(self->persist_state, handle);

  if (!self->super.buffer)
    {
      self->super.buffer = g_malloc(state->super.buffer_size);
    }
  state->super.pending_buffer_end = 0;

  if (state->file_inode &&
      state->file_inode == st.st_ino &&
      state->file_size <= st.st_size)
    {
      ofs = MIN(st.st_size, state->super.raw_stream_pos);

      lseek(fd, ofs, SEEK_SET);
    }
  else
    {
      if (state->file_inode)
        {
          /* the stored state does not match the current file */
          msg_notice("The current log file has a mismatching size/inode information, restarting from the beginning",
                     evt_tag_str("filename", self->filename),
                     evt_tag_int("stored_inode", state->file_inode),
                     evt_tag_int("cur_file_inode", st.st_ino),
                     evt_tag_int("stored_size", state->file_size),
                     evt_tag_int("cur_file_size", st.st_size),
                     NULL);
        }
      goto error;
    }
  if (state->super.raw_buffer_size)
    {
      gssize rc;
      guchar *raw_buffer;

      self->super.state = &state->super;
      if (state->super.raw_buffer_size > state->super.buffer_size)
        {
          msg_notice("Invalid raw_buffer_size member in LogProtoFileReader state, restarting from the beginning",
                     evt_tag_str("filename", self->filename),
                     NULL);
          goto error;
        }
      if (!self->super.super.encoding)
        {
          /* no conversion, we read directly into our buffer */
          raw_buffer = self->super.buffer;
        }
      else
        {
          raw_buffer = g_alloca(state->super.raw_buffer_size);
        }

      rc = log_transport_read(self->super.super.transport, raw_buffer, state->super.raw_buffer_size, NULL);
      if (rc != state->super.raw_buffer_size)
        {
          msg_notice("Error re-reading buffer contents of the file to be continued, restarting from the beginning",
                     evt_tag_str("filename", self->filename),
                     NULL);
          goto error;
        }

      state->super.pending_buffer_end = 0;
      if (self->super.super.encoding)
        {
          if (!log_proto_plain_server_convert_from_raw(&self->super, raw_buffer, rc))
            {
              msg_notice("Error re-converting buffer contents of the file to be continued, restarting from the beginning",
                         evt_tag_str("filename", self->filename),
                         NULL);
              goto error;
            }
        }
      else
        {
          self->super.state->pending_buffer_end += rc;
        }

      if (state->super.buffer_pos > state->super.pending_buffer_end ||
          state->super.buffer_cached_eol > state->super.pending_buffer_end)
        {
          msg_notice("Converted buffer contents is smaller than the current buffer position, starting from the beginning of the buffer, some lines may be duplicated",
                     evt_tag_str("filename", self->filename),
                     NULL);
          state->super.buffer_pos = state->super.pending_buffer_pos = state->super.buffer_cached_eol = 0;
        }
    }
  else
    {
      /* although we do have buffer position information, but the
       * complete contents of the buffer is already processed, instead
       * of reading and then dropping it, position the file after the
       * indicated block */

      state->super.raw_stream_pos += state->super.raw_buffer_size;
      ofs = state->super.raw_stream_pos;
      state->super.raw_buffer_size = 0;
      state->super.buffer_pos = state->super.pending_buffer_end = 0;

      lseek(fd, state->super.raw_stream_pos, SEEK_SET);
    }
  goto exit;

 error:
  ofs = 0;
  lseek(fd, 0, SEEK_SET);

 exit:
  state->file_inode = st.st_ino;
  state->file_size = st.st_size;
  state->super.raw_stream_pos = ofs;
  state->super.pending_buffer_pos = state->super.buffer_pos;
  state->super.pending_raw_stream_pos = state->super.raw_stream_pos;
  state->super.pending_raw_buffer_size = state->super.raw_buffer_size;

  self->super.state = NULL;
  persist_state_unmap_entry(self->persist_state, handle);
}

static PersistEntryHandle
log_proto_file_reader_alloc_state(LogProtoFileReader *self, PersistState *persist_state, const gchar *persist_name)
{
  LogProtoFileReaderState *state;
  PersistEntryHandle handle;

  handle = persist_state_alloc_entry(persist_state, persist_name, sizeof(LogProtoFileReaderState));
  if (handle)
    {
      state = persist_state_map_entry(persist_state, handle);

      state->super.version = 0;
      state->super.big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
      state->super.buffer_size = self->super.max_msg_size;

      persist_state_unmap_entry(persist_state, handle);

    }
  return handle;
}

static gboolean
log_proto_file_reader_convert_state(LogProtoFileReader *self, guint8 persist_version, gpointer old_state, gsize old_state_size, LogProtoFileReaderState *state)
{
  if (persist_version <= 2)
    {
      state->super.version = 0;
      state->file_inode = 0;
      state->super.raw_stream_pos = strtoll((gchar *) old_state, NULL, 10);
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

      if (!self->super.buffer || state->super.buffer_size < buffer_len)
        {
          gsize buffer_size = MAX(self->super.max_msg_size, buffer_len);
          self->super.buffer = g_realloc(self->super.buffer, buffer_size);
        }
      serialize_archive_free(archive);

      memcpy(self->super.buffer, buffer, buffer_len);
      state->super.buffer_pos = 0;
      state->super.pending_buffer_end = buffer_len;
      g_free(buffer);

      state->super.version = 0;
      state->file_inode = cur_inode;
      state->super.raw_stream_pos = cur_pos;
      state->file_size = cur_size;
      return TRUE;
    error_converting_v3:
      serialize_archive_free(archive);
    }
  return FALSE;
}

gboolean
log_proto_file_reader_restart_with_state(LogProto *s, PersistState *persist_state, const gchar *persist_name)
{
  LogProtoFileReader *self = (LogProtoFileReader *) s;
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
      new_state_handle = log_proto_file_reader_alloc_state(self, persist_state, persist_name);
      if (!new_state_handle)
        goto fallback_non_persistent;
      log_proto_file_reader_apply_state(self, new_state_handle);
      return TRUE;
    }
  if (persist_version < 4)
    {
      new_state_handle = log_proto_file_reader_alloc_state(self, persist_state, persist_name);
      if (!new_state_handle)
        goto fallback_non_persistent;

      old_state = persist_state_map_entry(persist_state, old_state_handle);
      new_state = persist_state_map_entry(persist_state, new_state_handle);
      success = log_proto_file_reader_convert_state(self, persist_version, old_state, old_state_size, new_state);
      persist_state_unmap_entry(persist_state, old_state_handle);
      persist_state_unmap_entry(persist_state, new_state_handle);

      /* we're using the newly allocated state structure regardless if
       * conversion succeeded. If the conversion went wrong then
       * new_state is still in its freshly initialized form since the
       * convert function will not touch the state in the error
       * branches.
       */

      log_proto_file_reader_apply_state(self, new_state_handle);
      return success;
    }
  else if (persist_version == 4)
    {
      LogProtoFileReaderState *state;

      old_state = persist_state_map_entry(persist_state, old_state_handle);
      state = old_state;
      if ((state->super.big_endian && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
          (!state->super.big_endian && G_BYTE_ORDER == G_BIG_ENDIAN))
        {

          /* byte order conversion in order to avoid the hassle with
             scattered byte order conversions in the code */

          state->super.big_endian = !state->super.big_endian;
          state->super.buffer_pos = GUINT32_SWAP_LE_BE(state->super.buffer_pos);
          state->super.pending_buffer_pos = GUINT32_SWAP_LE_BE(state->super.pending_buffer_pos);
          state->super.pending_buffer_end = GUINT32_SWAP_LE_BE(state->super.pending_buffer_end);
          state->super.buffer_size = GUINT32_SWAP_LE_BE(state->super.buffer_size);
          state->super.buffer_cached_eol = GUINT32_SWAP_LE_BE(state->super.buffer_cached_eol);
          state->super.raw_stream_pos = GUINT64_SWAP_LE_BE(state->super.raw_stream_pos);
          state->super.raw_buffer_size = GUINT32_SWAP_LE_BE(state->super.raw_buffer_size);
          state->super.pending_raw_stream_pos = GUINT64_SWAP_LE_BE(state->super.pending_raw_stream_pos);
          state->super.pending_raw_buffer_size = GUINT32_SWAP_LE_BE(state->super.pending_raw_buffer_size);
          state->file_size = GUINT64_SWAP_LE_BE(state->file_size);
          state->file_inode = GUINT64_SWAP_LE_BE(state->file_inode);
        }

      if (state->super.version > 0)
        {
          msg_error("Internal error restoring log reader state, stored data is too new",
                    evt_tag_int("version", state->super.version));
          goto error;
        }
      persist_state_unmap_entry(persist_state, old_state_handle);
      log_proto_file_reader_apply_state(self, old_state_handle);
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
  new_state = g_new0(LogProtoFileReaderState, 1);
 error:
  if (!new_state)
    {
      new_state_handle = log_proto_file_reader_alloc_state(self, persist_state, persist_name);
      if (!new_state_handle)
        goto fallback_non_persistent;
      new_state = persist_state_map_entry(persist_state, new_state_handle);
    }
  if (new_state)
    {
      LogProtoFileReaderState *state = new_state;

      /* error happened,  restart the file from the beginning */
      state->super.raw_stream_pos = 0;
      state->file_inode = 0;
      state->file_size = 0;
      if (new_state_handle)
        log_proto_file_reader_apply_state(self, new_state_handle);
      else
        self->super.state = new_state;
    }
  if (new_state_handle)
    {
      persist_state_unmap_entry(persist_state, new_state_handle);
    }
  return FALSE;
}

static gboolean
log_proto_file_reader_prepare(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  LogProtoFileReader *self = (LogProtoFileReader *) s;
  gboolean success;

  if (self->persist_handle)
    {
      self->super.state = persist_state_map_entry(self->persist_state, self->persist_handle);
    }
  success = log_proto_plain_server_prepare(s, fd, cond, timeout);
  if (self->persist_handle)
    {
      persist_state_unmap_entry(self->persist_state, self->persist_handle);
      self->super.state = NULL;
    }
  return success;
}

static LogProtoStatus
log_proto_file_reader_fetch(LogProto *s, const guchar **msg, gsize *msg_len, GSockAddr **sa, gboolean *may_read)
{
  LogProtoFileReader *self = (LogProtoFileReader *) s;
  LogProtoStatus result;

  if (self->persist_handle)
    {
      self->super.state = persist_state_map_entry(self->persist_state, self->persist_handle);
    }
  result = log_proto_plain_server_fetch(s, msg, msg_len, sa, may_read);
  if (self->persist_handle)
    {
      persist_state_unmap_entry(self->persist_state, self->persist_handle);
      self->super.state = NULL;
    }
  return result;
}

void
log_proto_file_reader_queued(LogProto *s)
{
  LogProtoFileReader *self = (LogProtoFileReader *) s;
  LogProtoFileReaderState *state;

  if (self->persist_handle)
    {
      self->super.state = persist_state_map_entry(self->persist_state, self->persist_handle);
    }
  state = (LogProtoFileReaderState *) self->super.state;
  log_proto_plain_server_queued(s);
  if (self->persist_handle)
    {
      persist_state_unmap_entry(self->persist_state, self->persist_handle);
      self->super.state = NULL;
    }

}

static void
log_proto_file_reader_free(LogProto *s)
{
  LogProtoFileReader *self = (LogProtoFileReader *) s;

  g_free(self->filename);
  log_proto_plain_server_free(s);
}

LogProto *
log_proto_file_reader_new(LogTransport *transport, const gchar *filename, gint padding_size, gint max_msg_size, guint flags)
{
  LogProtoFileReader *self = g_new0(LogProtoFileReader, 1);

  log_proto_plain_server_init(&self->super, transport, padding_size, max_msg_size, flags | LPPF_POS_TRACKING);

  self->super.super.prepare = log_proto_file_reader_prepare;
  self->super.super.fetch = log_proto_file_reader_fetch;
  self->super.super.queued = log_proto_file_reader_queued;
  self->super.super.free_fn = log_proto_file_reader_free;
  self->filename = g_strdup(filename);
  return &self->super.super;
}


#define LPFCS_FRAME_INIT    0
#define LPFCS_FRAME_SEND    1
#define LPFCS_MESSAGE_SEND  2

typedef struct _LogProtoFramedClient
{
  LogProtoPlainClient super;
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
      rc = log_proto_plain_client_post(s, msg, msg_len, consumed);
      
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
log_proto_framed_new_client(LogTransport *transport)
{
  LogProtoFramedClient *self = g_new0(LogProtoFramedClient, 1);

  self->super.super.prepare = log_proto_plain_client_prepare;  
  self->super.super.post = log_proto_framed_client_post;
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
      if (isdigit(self->buffer[i]))
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
log_proto_framed_new_server(LogTransport *transport, gint max_msg_size)
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

