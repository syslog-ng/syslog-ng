/*
 * Copyright (c) 2002-2007 BalaBit IT Ltd, Budapest, Hungary                    
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

#include "logreader.h"
#include "messages.h"
#include "fdread.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

static gboolean log_reader_fetch_log(LogReader *self, FDRead *fd);

typedef struct _LogReaderWatch
{
  GSource super;
  LogReader *reader;
  GPollFD pollfd;
  FDRead *fd;
} LogReaderWatch;

static gboolean
log_reader_fd_prepare(GSource *source,
                      gint *timeout)
{
  LogReaderWatch *self = (LogReaderWatch *) source;

  
#if 0
  /* FIXME: this debug message references a variable outside of its scope, 
   * but it is a debug message only */
  msg_debug("log_reader_fd_prepare()", 
            evt_tag_int("window_size", self->reader->options->source_opts.window_size), 
            NULL);
#endif

  self->pollfd.revents = 0;
  self->pollfd.events = 0;
  
  /* never indicate readability if flow control prevents us from sending messages */
  
  if (!log_source_free_to_send(&self->reader->super))
    return FALSE;
  
  /* always readable if we have a complete line in our buffer.  as the
   * reader is at a lower priority than writers, buffers are flushed prior
   * to new messages are read
   */
  
  if (self->reader->flags & LR_COMPLETE_LINE)
    return TRUE;
  
  if (self->reader->flags & LR_FOLLOW)
    {
      *timeout = self->reader->options->follow_freq * 1000;
      return FALSE;
    }
  
  self->pollfd.events = G_IO_IN;
  return FALSE;
}

static gboolean
log_reader_fd_check(GSource *source)
{
  LogReaderWatch *self = (LogReaderWatch *) source;

  if (self->reader->flags & LR_COMPLETE_LINE)
    return TRUE;
  
  if (self->reader->flags & LR_FOLLOW)
    {
      struct stat st, followed_st;
      off_t pos, size;
      
      /* check size, I cannot use stat, as glibc has a bug with file offset
       * bits 64 and struct stat, see
       * http://sourceware.org/bugzilla/show_bug.cgi?id=4328 */
      
      pos = lseek(self->fd->fd, 0, SEEK_CUR);
      size = lseek(self->fd->fd, 0, SEEK_END);
      lseek(self->fd->fd, pos, SEEK_SET);
      
      if (pos == (off_t) -1 || size == (off_t) -1)
        {
          msg_error("Error invoking seek on followed file",
                    evt_tag_errno("error", errno),
                    NULL);
          return FALSE;
        }
      
      if (pos < size)
	return TRUE;

      if (fstat(self->fd->fd, &st) < 0)
        {
          msg_error("Error invoking fstat() on followed file",
                    evt_tag_errno("error", errno),
                    NULL);
          return FALSE;
        }
      
      if (self->reader->options->follow_filename && stat(self->reader->options->follow_filename, &followed_st) != -1)
        {
          if (st.st_ino != followed_st.st_ino)
            {
              /* file was moved and we are at EOF, follow the new file */
              log_pipe_notify(self->reader->control, &self->reader->super.super, NC_FILE_MOVED, self);
              return TRUE;
            }
          return FALSE;
        }
      
      return FALSE;
    }
    
  return !!(self->pollfd.revents & (G_IO_IN | G_IO_ERR | G_IO_HUP));
}

static gboolean
log_reader_fd_dispatch(GSource *source,
                       GSourceFunc callback,
                       gpointer user_data)
{
  LogReaderWatch *self = (LogReaderWatch *) source;

  if (!log_reader_fetch_log(self->reader, self->fd))
    {
      return FALSE;
    }
    
  return TRUE;
}

static void
log_reader_fd_finalize(GSource *source)
{
  LogReaderWatch *self = (LogReaderWatch *) source;

  fd_read_free(self->fd);
  log_pipe_unref(&self->reader->super.super);
}

GSourceFuncs log_reader_source_funcs =
{
  log_reader_fd_prepare,
  log_reader_fd_check,
  log_reader_fd_dispatch,
  log_reader_fd_finalize
};

static GSource *
log_reader_watch_new(LogReader *reader, FDRead *fd)
{
  LogReaderWatch *self = (LogReaderWatch *) g_source_new(&log_reader_source_funcs, sizeof(LogReaderWatch));
  
  log_pipe_ref(&reader->super.super);
  self->reader = reader;
  self->fd = fd;
  self->pollfd.fd = fd->fd;
  g_source_set_priority(&self->super, LOG_PRIORITY_READER);
  g_source_add_poll(&self->super, &self->pollfd);
  return &self->super;
}

static gboolean
log_reader_handle_line(LogReader *self, gchar *line, gint length, GSockAddr *saddr, guint parse_flags)
{
  LogMessage *m;
  
  msg_debug("Incoming log entry", 
            evt_tag_printf("line", "%.*s", length, line),
            NULL);
      
  m = log_msg_new(line, length, saddr, parse_flags, self->options->bad_hostname);
  
  if (self->options->prefix)
    g_string_prepend(&m->msg, self->options->prefix);
      
  if (m->stamp.zone_offset == -1)
    m->stamp.zone_offset = self->options->zone_offset;
  if (!self->options->keep_timestamp)
    m->stamp = m->recvd;
  log_pipe_queue(&self->super.super, m, 0);
  
  return log_source_free_to_send(&self->super);
}

/**
 * log_reader_iterate_buf:
 * @self: LogReader instance
 * @saddr: socket address to be assigned to new messages (consumed!)
 * @flush: whether to flush the input buffer
 * @msg_counter: the number of messages processed in the current poll iteration
 * 
 **/
static gboolean
log_reader_iterate_buf(LogReader *self, GSockAddr *saddr, gboolean flush, gint *msg_count)
{
  gchar *eol, *start;
  gint length;
  gboolean may_read = TRUE;
  guint parse_flags;

  self->flags &= ~LR_COMPLETE_LINE;
  eol = memchr(self->buffer, '\0', self->ofs);
  if (eol == NULL)
    eol = memchr(self->buffer, '\n', self->ofs);
    
  parse_flags = 0;
  if (self->options->options & LRO_NOPARSE)
    parse_flags |= LP_NOPARSE;
  if (self->options->options & LRO_CHECK_HOSTNAME)
    parse_flags |= LP_CHECK_HOSTNAME;
  if (self->options->options & LRO_KERNEL)
    parse_flags = LP_KERNEL;
  if (self->flags & LR_STRICT)
    parse_flags |= LP_STRICT;
  if (self->flags & LR_INTERNAL)
    parse_flags |= LP_INTERNAL;
  if (self->flags & LR_LOCAL)
    parse_flags |= LF_LOCAL;
    
  if ((self->flags & LR_PKTTERM) || 
      (!eol && (self->ofs == self->options->msg_size)) || 
      self->options->padding ||
      flush) 
    {
      /* our buffer is full, or
       * we are set to packet terminating mode, or
       * we are in padded mode HP-UX
       */
      length = (self->options->padding 
                  ? (eol ? eol - self->buffer : self->ofs)
                  : self->ofs);
      if (length)
        log_reader_handle_line(self, self->buffer, length, saddr, parse_flags);
      self->ofs = 0;
      (*msg_count)++;
    }
  else
    {
      if (saddr == NULL)
        {
          saddr = self->prev_addr;
          self->prev_addr = NULL;
        }
      if (self->prev_addr)
        {
          g_sockaddr_unref(self->prev_addr);
          self->prev_addr = NULL;
        }
      start = self->buffer;
      while ((self->options->fetch_limit == 0 || (*msg_count) < self->options->fetch_limit) && eol && may_read)
	{
	  gchar *end = eol;
	  /* eol points at the newline character. end points at the
	   * character terminating the line, which may be a carriage
	   * return preceeding the newline. */
	   
	  (*msg_count)++;
	  
	  while ((end > start) && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == 0))
	    end--;
	  
	  length = end - start;
	  
	  if (length)
	    may_read = log_reader_handle_line(self, start, length, saddr, parse_flags);
	  
	  start = eol + 1;

	  eol = memchr(start, '\0', &self->buffer[self->ofs] - start);
	  if (eol == NULL)
	    eol = memchr(start, '\n', &self->buffer[self->ofs] - start);
	}
      
      /* move remaining data to the beginning of the buffer */
      
      if (eol)
        self->flags |= LR_COMPLETE_LINE;
      
      self->ofs = &self->buffer[self->ofs] - start;
      memmove(self->buffer, start, self->ofs);
      if (self->ofs != 0)
        self->prev_addr = g_sockaddr_ref(saddr);
    }

  return TRUE;
}


static gboolean
log_reader_fetch_log(LogReader *self, FDRead *fd)
{
  guint avail = self->options->msg_size - self->ofs;
  gint rc;
  GSockAddr *sa = NULL;
  gint msg_count = 0;

  /* iterare on previously buffered data */
  if (self->flags & LR_COMPLETE_LINE)
    {
      log_reader_iterate_buf(self, NULL, FALSE, &msg_count);
      
      /* we still have something */
      if (self->ofs != 0)
        return TRUE;
    }
  
  /* check for new data */
  if (self->options->padding)
    {
      if (avail < self->options->padding)
	{
	  msg_error("Buffer is too small to hold padding number of bytes",
	            evt_tag_int(EVT_TAG_FD, fd->fd),
	            evt_tag_int("padding", self->options->padding),
                    evt_tag_int("avail", avail),
                    NULL);
          log_pipe_notify(self->control, &self->super.super, NC_CLOSE, self);
	  return FALSE;
	}
      avail = self->options->padding;
    }
  
  /* NOTE: this loop is here to decrease the load on the main loop, we try
   * to fetch a couple of messages in a single run (but only up to
   * fetch_limit).
   */
  while (msg_count < self->options->fetch_limit)
    {
      avail = self->options->msg_size - self->ofs;
      sa = NULL;
      rc = fd_read(fd, self->buffer + self->ofs, avail, &sa);

      if (rc == -1)
        {
          g_sockaddr_unref(sa);
          if (errno == EAGAIN)
            {
              /* ok we don't have any more data to read, return to main poll loop */
              break;
            }
          else
            {
              /* an error occurred while reading */
              msg_error("I/O error occurred while reading",
                        evt_tag_int(EVT_TAG_FD, fd->fd),
                        evt_tag_errno(EVT_TAG_OSERROR, errno),
                        NULL);
              log_reader_iterate_buf(self, NULL, TRUE, &msg_count);
              log_pipe_notify(self->control, &self->super.super, NC_READ_ERROR, self);
              return FALSE;
            }
        }
      else if (rc == 0 && (self->flags & (LR_FOLLOW + LR_PKTTERM)) == 0)
        {
          /* EOF read */
          msg_verbose("EOF occurred while reading", 
                      evt_tag_int(EVT_TAG_FD, fd->fd),
                      NULL);
          log_reader_iterate_buf(self, NULL, TRUE, &msg_count);
          log_pipe_notify(self->control, &self->super.super, NC_CLOSE, self);
          g_sockaddr_unref(sa);
          return FALSE;
        }
      else 
        {
          if (self->options->padding && rc != self->options->padding)
            {
              msg_error("Padding was set, and couldn't read enough bytes",
                        evt_tag_int(EVT_TAG_FD, fd->fd),
                        evt_tag_int("padding", self->options->padding),
                        evt_tag_int("read", avail),
                        NULL);
              log_pipe_notify(self->control, &self->super.super, NC_READ_ERROR, self);
              g_sockaddr_unref(sa);
              return FALSE;
            }
          self->ofs += rc;
          log_reader_iterate_buf(self, sa, FALSE, &msg_count);
        }
      
      g_sockaddr_unref(sa);
      if (self->flags & LR_NOMREAD)
        break;
    }
  return TRUE;
}

static gboolean
log_reader_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogReader *self = (LogReader *) s;
  
  /* the source added below references this logreader, it will be unref'd
     when the source is destroyed */ 
  self->source = log_reader_watch_new(self, self->fd);
  g_source_attach(self->source, NULL);
  return TRUE;
}

static gboolean
log_reader_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogReader *self = (LogReader *) s;
  
  if (self->source)
    {
      g_source_destroy(self->source);
      g_source_unref(self->source);
      self->source = NULL;
    }
  return TRUE;
}

static void
log_reader_free(LogPipe *s)
{
  LogReader *self = (LogReader *) s;
  
  /* when this function is called the source is already removed, because it
     holds a reference to this reader */
  log_pipe_unref(self->control);
  g_free(self->buffer);
  g_free(s);
}

void
log_reader_set_options(LogPipe *s, LogReaderOptions *options)
{
  LogReader *self = (LogReader *) s;
  self->super.options = &options->source_opts;
  self->options = options;

  if (options->follow_freq > 0)
    self->flags |= LR_FOLLOW;
}

/**
 * log_reader_set_pos:
 *
 * This function sets the current read position of the associated fd. It
 * should be used as a first operation before any message is read and is
 * useful to restart at a specific file position. It is not intended to be
 * used on anything non-seekable (e.g. sockets).
 **/
void
log_reader_set_pos(LogReader *self, off_t ofs)
{
  off_t size;
  
  size = lseek(self->fd->fd, 0, SEEK_END);
  if (ofs > size)
    ofs = size;
  if (lseek(self->fd->fd, ofs, SEEK_SET) >= 0)
    self->ofs = 0;
}

/**
 * log_reader_get_pos:
 *
 * This function returns the current read position of the associated fd. It
 * should be used to query the current read position right before exiting. 
 * The returned value is adjusted with the bytes already read from the
 * stream but still in the input buffer. It is not intended to be used on
 * anything non-seekable (e.g. sockets)
 **/
off_t
log_reader_get_pos(LogReader *self)
{
  off_t res;
  
  res = lseek(self->fd->fd, 0, SEEK_CUR);
  
  if (res >= 0)
    return res - self->ofs;
  return res;
}

LogPipe *
log_reader_new(FDRead *fd, guint32 flags, LogPipe *control, LogReaderOptions *options)
{
  LogReader *self = g_new0(LogReader, 1);

  log_source_init_instance(&self->super, &options->source_opts);
  self->super.super.init = log_reader_init;
  self->super.super.deinit = log_reader_deinit;
  self->super.super.free_fn = log_reader_free;
  self->options = options;
  self->flags = flags;
  self->fd = fd;
  log_pipe_ref(control);
  self->control = control;
  self->buffer = g_malloc(options->msg_size);
  if (options->follow_freq > 0)
    self->flags |= LR_FOLLOW;
  return &self->super.super;
}

void
log_reader_options_defaults(LogReaderOptions *options)
{
  log_source_options_defaults(&options->source_opts);
  options->padding = 0;
  options->options = 0;
  options->fetch_limit = -1;
  options->msg_size = -1;
  options->follow_freq = -1; 
  options->zone_offset = -1;
  options->keep_timestamp = -1;
  options->bad_hostname = NULL;
}

void
log_reader_options_init(LogReaderOptions *options, GlobalConfig *cfg)
{
  log_source_options_init(&options->source_opts, cfg);
  if (options->fetch_limit == -1)
    options->fetch_limit = cfg->log_fetch_limit;
  if (options->msg_size == -1)
    options->msg_size = cfg->log_msg_size;
  if (options->follow_freq == -1)
    options->follow_freq = cfg->follow_freq;
  if (options->zone_offset == -1)
    options->zone_offset = cfg->recv_zone_offset;
  if (options->keep_timestamp == -1)
    options->keep_timestamp = cfg->keep_timestamp;
  options->options |= LRO_CHECK_HOSTNAME * (!!cfg->check_hostname);
  if (cfg->bad_hostname_compiled)
    options->bad_hostname = &cfg->bad_hostname;
}

