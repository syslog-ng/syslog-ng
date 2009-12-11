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

#include <ctype.h>
#include <string.h>

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

struct _LogProtoPlainServer
{
  LogProto super;
  guchar *buffer;
  const guchar *buffer_cached_eol;
  gsize buffer_size, buffer_end, buffer_pos;
  gsize padding_size, max_msg_size;
  GSockAddr *prev_saddr;
  gchar raw_buffer_leftover[8];
  gint raw_buffer_leftover_size;
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
  return (self->buffer_cached_eol == NULL);
}

static gboolean 
log_proto_plain_server_read_state(LogProto *s, SerializeArchive *archive)
{
  LogProtoPlainServer *self = (LogProtoPlainServer *) s;
  gchar *buffer;
  gsize buffer_len;
  guint16 version;
  
  log_proto_reset_state(s);
  
  if (!serialize_read_uint16(archive, &version) || version != 0)
    {
      msg_error("Internal error, protocol state has incorrect version",
                evt_tag_int("version", version),
                NULL);
      return FALSE;
    }
  
  if (!serialize_read_cstring(archive, &buffer, &buffer_len))
    return FALSE;
  if (self->buffer_size < buffer_len)
    {
      g_free(self->buffer);
      self->buffer_size = MAX(8192, buffer_len);
      self->buffer = g_malloc(self->buffer_size);
    }
  memcpy(self->buffer, buffer, buffer_len);
  self->buffer_pos = 0;
  self->buffer_end = buffer_len;
  g_free(buffer);
  
  return TRUE;
}

static gboolean 
log_proto_plain_server_write_state(LogProto *s, SerializeArchive *archive)
{
  LogProtoPlainServer *self = (LogProtoPlainServer *) s;
  gint version = 0;
  
  /* NOTE: we don't support serializing prev_saddr as this functionality is only
   * used to serialize file related information */
  
  g_assert(self->prev_saddr == NULL);
  
  return serialize_write_uint16(archive, version) &&
         serialize_write_cstring(archive, (gchar *) &self->buffer[self->buffer_pos], self->buffer_end - self->buffer_pos);
}

static void
log_proto_plain_server_reset_state(LogProto *s)
{
  LogProtoPlainServer *self = (LogProtoPlainServer *) s;

  self->buffer_end = self->buffer_pos = 0;
  self->buffer_cached_eol = NULL;
  g_sockaddr_unref(self->prev_saddr);
  self->prev_saddr = NULL;
  self->status = LPS_SUCCESS;
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

  return self->buffer_cached_eol != NULL;
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
          char_ptr = (const guchar *) (longword_ptr - 1);
          gint i;
          
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

  buffer_start = self->buffer + self->buffer_pos;
  buffer_bytes = self->buffer_end - self->buffer_pos;

  if (buffer_bytes == 0)
    return FALSE;

  if ((self->super.flags & LPPF_PKTTERM) || flush_the_rest)
    {
      /*
       * we are set to packet terminating mode or the connection is to
       * be teared down and we have partial data in our buffer.
       */
      *msg = buffer_start;
      *msg_len = buffer_bytes;
      self->buffer_end = 0;
      self->buffer_pos = 0;
      return TRUE;
    }
  
  if (self->buffer_cached_eol)
    {
      /* previous invocation was nice enough to save a cached EOL
       * pointer, no need to look it up again */

      eol = self->buffer_cached_eol;
      self->buffer_cached_eol = NULL;
    }
  else
    {
      eol = find_eom(buffer_start, buffer_bytes);
    }
  if ((!eol && (buffer_bytes == self->buffer_size)) || self->padding_size) 
    {
      /* our buffer is full and no EOL was found, or we're in HP-UX padded mode */
      *msg_len = (self->padding_size 
                  ? (eol ? eol - buffer_start : buffer_bytes)
                  : buffer_bytes);
      self->buffer_pos = 0;
      self->buffer_end = 0;
      *msg = buffer_start;
      return TRUE;
    }
  else if (!eol)
    {
      /* buffer is not full, but no EOL is present, move partial line
       * to the beginning of the buffer to make space for new data.
       */
      memmove(self->buffer, buffer_start, buffer_bytes);
      self->buffer_pos = 0;
      self->buffer_end = buffer_bytes;
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
      self->buffer_pos = eol + 1 - self->buffer;
      
      if (self->buffer_end != self->buffer_pos)
        {
          /* store the end of the next line, it indicates whether we need
           * to read further data, or the buffer already contains a
           * complete line */
          self->buffer_cached_eol = find_eom(self->buffer + self->buffer_pos, self->buffer_end - self->buffer_pos);
        }
      else
        {
          self->buffer_end = self->buffer_pos = 0;
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
  gchar *raw_buffer = NULL;

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

      if (self->super.convert == (GIConv) -1)
        {
          /* no conversion, we read directly into our buffer */
          raw_buffer = (gchar *) self->buffer + self->buffer_end;
          if (!self->padding_size)
            avail = self->buffer_size - self->buffer_end;
          else
            {

              /* data is read/processed in "padding" sized chunks,
               * thus iff padding > buffer_size we never fail this
               * assertion */
              g_assert(self->buffer_size - self->buffer_end >= self->padding_size);
              avail = self->padding_size;
            }
        }
      else
        {
          /* if conversion is needed, we first read into an on-stack
           * buffer, and then convert it into our internal buffer */

          raw_buffer = g_alloca(self->max_msg_size + self->raw_buffer_leftover_size);
          memcpy(raw_buffer, self->raw_buffer_leftover, self->raw_buffer_leftover_size);
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

      rc = log_transport_read(self->super.transport, raw_buffer + self->raw_buffer_leftover_size, avail, sa);
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
              if (self->raw_buffer_leftover_size > 0)
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
          rc += self->raw_buffer_leftover_size;
          self->raw_buffer_leftover_size = 0;

          /* some data was read */
          if (self->super.convert != (GIConv) -1)
            {
              gsize avail_in = rc;
              gsize avail_out;
              gchar *out;
              gint  ret = -1;

              do
                {
                  avail_out = self->buffer_size - self->buffer_end;
                  out = (gchar *) self->buffer + self->buffer_end;

                  ret = g_iconv(self->super.convert, (gchar **) &raw_buffer, &avail_in, (gchar **) &out, &avail_out);
                  if (ret == (gsize) -1)
                    {
                      switch (errno)
                        {
                        case EINVAL:
                          /* Incomplete text, do not report an error, rather try to read again */
                          self->buffer_end = self->buffer_size - avail_out;

                          if (avail_in > 0)
                            {
                              if (avail_in > sizeof(self->raw_buffer_leftover))
                                {
                                  msg_error("Invalid byte sequence, the remaining raw buffer is larger than the supported leftover size",
                                            evt_tag_str("encoding", self->super.encoding),
                                            evt_tag_int("avail_in", avail_in),
                                            evt_tag_int("leftover_size", sizeof(self->raw_buffer_leftover)),
                                            NULL);
                                  self->status = LPS_ERROR;
                                  return self->status;
                                }
                              memcpy(self->raw_buffer_leftover, raw_buffer, avail_in);
                              self->raw_buffer_leftover_size = avail_in;
                              msg_debug("Leftover characters remained after conversion, delaying message until another chunk arrives",
                                        evt_tag_str("encoding", self->super.encoding),
                                        evt_tag_int("avail_in", avail_in),
                                        NULL);
                              return LPS_SUCCESS;
                            }
                          break;
                        case E2BIG:
                          
                          self->buffer_end = self->buffer_size - avail_out;
                          /* extend the buffer */
                          
                          if (self->buffer_size < self->max_msg_size * 6)
                            {
                              self->buffer_size *= 2;
                              self->buffer = g_realloc(self->buffer, self->buffer_size);

                              /* recalculate the out pointer, and add what we have now */
                              ret = -1;
                            }
                          else
                            {
                              msg_error("Incoming byte stream requires a too large conversion buffer, probably invalid character sequence",
                                        evt_tag_str("encoding", self->super.encoding),
                                        evt_tag_printf("buffer", "%.*s", (gint) self->buffer_end, self->buffer),
                                        NULL);
                              self->status = LPS_ERROR;
                              return self->status;
                            }
                          break;
                        case EILSEQ:
                        default:
                          msg_notice("Invalid byte sequence or other error while converting input, skipping character",
                                     evt_tag_str("encoding", self->super.encoding),
                                     evt_tag_printf("char", "0x%02x", *(guchar *) raw_buffer),
                                     NULL);
                          self->buffer_end = self->buffer_size - avail_out;
                          raw_buffer++;
                          avail_in--;
                          break;
                        }
                    }
                  else
                    {
                      self->buffer_end = self->buffer_size - avail_out;
                    }
                }
              while (avail_in > 0);
            }
          else
            {
              self->buffer_end += rc;
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
log_proto_plain_server_free(LogProto *s)
{
  LogProtoPlainServer *self = (LogProtoPlainServer *) s;

  g_sockaddr_unref(self->prev_saddr);
  g_free(self->buffer);
}

LogProto *
log_proto_plain_new_server(LogTransport *transport, gint padding_size, gint max_msg_size, guint flags)
{
  LogProtoPlainServer *self = g_new0(LogProtoPlainServer, 1);

  self->super.flags = flags;
  self->super.read_state = log_proto_plain_server_read_state;
  self->super.write_state = log_proto_plain_server_write_state;
  self->super.reset_state = log_proto_plain_server_reset_state;
  
  self->super.prepare = log_proto_plain_server_prepare;  
  self->super.fetch = log_proto_plain_server_fetch;
  self->super.free_fn = log_proto_plain_server_free;
  self->super.transport = transport;
  self->super.convert = (GIConv) -1;
  self->padding_size = padding_size;
  self->max_msg_size = max_msg_size;
  self->buffer_size = max_msg_size;
  self->buffer = g_malloc(max_msg_size);
  return &self->super;
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
  GSockAddr *prev_saddr;
  LogProtoStatus status;
} LogProtoFramedServer;

static gboolean
log_proto_framed_server_prepare(LogProto *s, gint *fd, GIOCondition *cond, gint *timeout)
{
  LogProtoPlainServer *self = (LogProtoPlainServer *) s;
  
  *fd = self->super.transport->fd;
  *cond = self->super.transport->cond;

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
    }
  else if (rc == 0)
    {
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
  return &self->super;
}

