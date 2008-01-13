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
  
#include "logwriter.h"
#include "logqueue.h"
#include "messages.h"
#include "macros.h"
#include "fdwrite.h"
#include "stats.h"
#include "misc.h"

#include <unistd.h>
#include <assert.h>

typedef struct _LogWriterWatch
{
  GSource super;
  GPollFD pollfd;
  LogWriter *writer;
  FDWrite *fd;
  GTimeVal flush_target;
  GTimeVal last_throttle_check;
  gboolean flush_waiting_for_timeout:1,
           input_means_connection_broken:1;
} LogWriterWatch;

/**
 * LogWriter behaviour
 * ~~~~~~~~~~~~~~~~~~~
 *
 * LogWriter is a core element of syslog-ng sending messages out to some
 * kind of destination represented by a UNIX fd. Outgoing messages are sent
 * to the target asynchronously, first by placing them to a queue and then
 * sending messages when poll() indicates that the fd is writable.
 *
 * 
 * Flow control
 * ------------
 * For a simple log writer without a disk buffer messages are placed on a
 * GQueue and they are acknowledged when the send() system call returned
 * success. This is more complex when disk buffering is used, in which case
 * messages are put to the "disk buffer" first and acknowledged immediately. 
 * (this way the reader never stops when the disk buffer area is not yet
 * full). When disk buffer reaches its limit, messages are added to the the
 * usual GQueue and messages get acknowledged when they are moved to the
 * disk buffer.
 *
 **/

static gboolean log_writer_flush_log(LogWriter *self, FDWrite *fd);
static void log_writer_broken(LogWriter *self, gint notify_code);
static gboolean log_writer_throttling(LogWriter *self);


static gboolean
log_writer_fd_prepare(GSource *source,
                      gint *timeout)
{
  LogWriterWatch *self = (LogWriterWatch *) source;
  gint64 num_elements = log_queue_get_length(self->writer->queue);
  
  /* recalculate buckets */
  
  if (self->writer->options->throttle > 0)
    {
      GTimeVal now;
      gint64 diff;
      gint new_buckets;
      
      /* throttling is enabled, calculate new buckets */
      
      g_source_get_current_time(source, &now);
      if (self->last_throttle_check.tv_sec != 0)
        {
          diff = g_time_val_diff(&now, &self->last_throttle_check);
        }
      else
        {
          diff = 0;
          self->last_throttle_check = now;
        }
      new_buckets = (self->writer->options->throttle * diff) / G_USEC_PER_SEC;
      if (new_buckets)
        {
          
          /* if new_buckets is zero, we don't save the current time as
           * last_throttle_check. The reason is that new_buckets could be
           * rounded to zero when only a minimal interval passes between
           * poll iterations.
           */
          self->writer->throttle_buckets = MIN(self->writer->options->throttle, self->writer->throttle_buckets + new_buckets);
          self->last_throttle_check = now;
        }
    }

  if (self->writer->partial ||
      (self->writer->options->flush_lines == 0 && (!log_writer_throttling(self->writer) && num_elements != 0)) ||
      (self->writer->options->flush_lines > 0  && (!log_writer_throttling(self->writer) && num_elements >= self->writer->options->flush_lines)))
    {
      /* we need to flush our buffers */
      self->pollfd.events = self->fd->cond;
    }
  else if (num_elements && !log_writer_throttling(self->writer))
    {
      /* our buffer does not contain enough elements to flush, but we do not
       * want to wait more than flush_timeout time */
      
      if (!self->flush_waiting_for_timeout)
        {
          /* start waiting */

          *timeout = self->writer->options->flush_timeout;
          g_source_get_current_time(source, &self->flush_target);
          g_time_val_add(&self->flush_target, *timeout * 1000);
          self->flush_waiting_for_timeout = TRUE;
        }
      else
        {
          GTimeVal now, diff;

          g_source_get_current_time(source, &now);
          
          diff.tv_sec = self->flush_target.tv_sec - now.tv_sec;
          diff.tv_usec = self->flush_target.tv_usec - now.tv_usec;
          if (diff.tv_usec < 0)
            {
              diff.tv_sec--;
              diff.tv_usec += 1000000;
            }
          else if (diff.tv_usec > 1000000)
            {
              diff.tv_sec++;
              diff.tv_usec -= 1000000;
            }
          /* we already started to wait for flush, recalculate next timeout */
          *timeout = diff.tv_sec * 1000 + diff.tv_usec / 1000;
          if (*timeout < 0)
            return TRUE;
        }
      return FALSE;
    }
  else
    {
      self->pollfd.events = 0;
      if (num_elements && log_writer_throttling(self->writer))
        {
          /* we are unable to send because of throttling, make sure that we
           * wake up when the rate limits lets us send at least 1 message */
          *timeout = (1000 / self->writer->options->throttle) + 1;
          msg_debug("Throttling output", 
                    evt_tag_int("wait", *timeout), 
                    NULL);
        }
    }
  
  if (self->writer->flags & LW_DETECT_EOF && (self->pollfd.events & G_IO_IN) == 0)
    {
      self->pollfd.events |= G_IO_HUP | G_IO_IN;
      self->input_means_connection_broken = TRUE;
    }
  else
    {
      self->input_means_connection_broken = FALSE;
    }
  self->flush_waiting_for_timeout = FALSE;
  self->pollfd.revents = 0;
  return FALSE;
}

static gboolean
log_writer_fd_check(GSource *source)
{
  LogWriterWatch *self = (LogWriterWatch *) source;
  gint64 num_elements = log_queue_get_length(self->writer->queue);
  
  if ((num_elements && !log_writer_throttling(self->writer)) || self->writer->partial)
    {
      /* we have data to flush */
      if (self->flush_waiting_for_timeout)
        {
          GTimeVal tv;
          /* check if timeout elapsed */
          g_source_get_current_time(source, &tv);
          return self->flush_target.tv_sec <= tv.tv_sec || (self->flush_target.tv_sec == tv.tv_sec && self->flush_target.tv_usec <= tv.tv_usec);
        }
    }
  return !!(self->pollfd.revents & (G_IO_OUT | G_IO_ERR | G_IO_HUP | G_IO_IN));
}

static gboolean
log_writer_fd_dispatch(GSource *source,
		       GSourceFunc callback,
		       gpointer user_data)
{
  LogWriterWatch *self = (LogWriterWatch *) source;
  gint64 num_elements = log_queue_get_length(self->writer->queue);

  if (self->pollfd.revents & (G_IO_HUP | G_IO_IN) && self->input_means_connection_broken)
    {
      msg_error("EOF occurred while idle",
                evt_tag_int("fd", self->fd->fd),
                NULL);
      log_writer_broken(self->writer, NC_CLOSE);
      return FALSE;
    }
  else if (num_elements || self->writer->partial)
    {
      if (!log_writer_flush_log(self->writer, self->fd))
        return FALSE;
    }
  return TRUE;
}

static void
log_writer_fd_finalize(GSource *source)
{
  LogWriterWatch *self = (LogWriterWatch *) source;

  fd_write_free(self->fd);
  log_pipe_unref(&self->writer->super);
}

GSourceFuncs log_writer_source_funcs =
{
  log_writer_fd_prepare,
  log_writer_fd_check,
  log_writer_fd_dispatch,
  log_writer_fd_finalize
};

static GSource *
log_writer_watch_new(LogWriter *writer, FDWrite *fd)
{
  LogWriterWatch *self = (LogWriterWatch *) g_source_new(&log_writer_source_funcs, sizeof(LogWriterWatch));
  
  self->writer = writer;
  self->fd = fd;
  log_pipe_ref(&self->writer->super);
  self->pollfd.fd = fd->fd;
  g_source_set_priority(&self->super, LOG_PRIORITY_WRITER);
  g_source_add_poll(&self->super, &self->pollfd);
  return &self->super;
}

static gboolean
log_writer_throttling(LogWriter *self)
{
  return self->options->throttle > 0 && self->throttle_buckets == 0;
}


static void
log_writer_queue(LogPipe *s, LogMessage *lm, gint path_flags)
{
  LogWriter *self = (LogWriter *) s;
  
  if (!log_queue_push_tail(self->queue, lm, path_flags))
    {
      /* drop incoming message, we must ack here, otherwise the sender might
       * block forever, however this should not happen unless the sum of
       * window_sizes of sources feeding this writer exceeds log_fifo_size
       * or if flow control is not turned on.
       */
      
      /* we don't send a message here since the system is draining anyway */
      
      if (self->dropped_messages)
        (*self->dropped_messages)++;
      msg_debug("Destination queue full, dropping message",
                evt_tag_int("queue_len", log_queue_get_length(self->queue)),
                evt_tag_int("mem_fifo_size", self->options->mem_fifo_size),
                NULL);
      log_msg_drop(lm, path_flags);
      return;
    }
}

void
log_writer_format_log(LogWriter *self, LogMessage *lm, GString *result)
{
  LogTemplate *template = NULL;
  
  if (self->options->template)
    {
      template = self->options->template;
    }
  else if (self->flags & LW_FORMAT_FILE)
    {
      template = self->options->file_template;
    }
  else if ((self->flags & LW_FORMAT_PROTO))
    {
      template = self->options->proto_template;
    }
  
  if (template)
    {
      log_template_format(template, lm, 
                          ((self->options->options & LWO_TMPL_ESCAPE) ? MF_ESCAPE_RESULT : 0) |
		          (self->options->use_time_recvd ? MF_STAMP_RECVD : 0), 
		          self->options->ts_format,
		          self->options->zone_offset,
		          self->options->frac_digits,
		          result);

    }
  else 
    {
      GString *ts;
      LogStamp *stamp;

      /* no template was specified, use default */

      if (self->options->use_time_recvd)
        stamp = &lm->recvd;
      else
        stamp = &lm->stamp;

      ts = g_string_sized_new(64);
      if (self->flags & LW_FORMAT_FILE)
        {
          log_stamp_format(stamp, ts, self->options->ts_format, self->options->zone_offset, self->options->frac_digits);
          g_string_sprintf(result, "%s %s %s\n", ts->str, lm->host.str, lm->msg.str);
        }
      else if (self->flags & LW_FORMAT_PROTO)
        {
          /* always use BSD timestamp by default, the use can override this using a custom template */
          log_stamp_format(stamp, ts, TS_FMT_BSD, self->options->zone_offset, self->options->frac_digits);
          g_string_sprintf(result, "<%d>%s %s %s\n", lm->pri, ts->str, lm->host.str, lm->msg.str);
        }
      g_string_free(ts, TRUE);
    }
}

static void
log_writer_broken(LogWriter *self, gint notify_code)
{
  /* the order of these calls is important, as log_pipe_notify() will handle
   * reinitialization, and if deinit is called last, the writer might be
   * left in an unpolled state */
  
  log_pipe_deinit(&self->super, NULL, NULL);
  log_pipe_notify(self->control, &self->super, notify_code, self);
}

static gboolean 
log_writer_flush_log(LogWriter *self, FDWrite *fd)
{
  GString *line = NULL;
  gint rc;
  gint64 num_elements;
  
  if (self->partial)
    {
      int len = self->partial->len - self->partial_pos;
      
      rc = fd_write(fd, &self->partial->str[self->partial_pos], len);
      if (rc == -1)
        {
          if (errno != EAGAIN)
            goto write_error;
          else
            return TRUE;
        }
      else if (rc != len)
        {
          self->partial_pos += rc;
          return TRUE;
        }
      else
        {
          line = self->partial;
          self->partial = NULL;
        }
    }
  else
    {  
      line = g_string_sized_new(128);
    }
  num_elements = log_queue_get_length(self->queue);
  while (num_elements > 0 && !log_writer_throttling(self))
    {
      LogMessage *lm;
      gint path_flags;
      
      if (!log_queue_pop_head(self->queue, &lm, &path_flags))
        g_assert_not_reached();

      log_writer_format_log(self, lm, line);
      
      log_msg_ack(lm, path_flags);
      log_msg_unref(lm);
      
      /* account this message against the throttle rate */
      self->throttle_buckets--;
      
      if (line->len)
        {
          rc = fd_write(fd, line->str, line->len);
          
          if (rc == -1)
            {
              self->partial = line;
              self->partial_pos = 0;
              line = NULL;
              if (errno != EAGAIN)
                goto write_error;
              return TRUE;
            }
          else 
            {
 
              if (rc != line->len)
                {
                  /* partial flush */
                  self->partial = line;
                  self->partial_pos = rc;
                  return TRUE;
                }
            }
        }
      num_elements--;
    }
  g_string_free(line, TRUE);
  return TRUE;

write_error:
  {
    /*    int error = errno; */
    /* error flushing data */
    if (errno != EAGAIN && errno != EINTR)
      {
        msg_error("I/O error occurred while writing",
                  evt_tag_int("fd", fd->fd),
                  evt_tag_errno(EVT_TAG_OSERROR, errno),
                  NULL);
        log_writer_broken(self, NC_WRITE_ERROR);
        if (line)
          g_string_free(line, TRUE);
        return FALSE;
      }
    else
      return TRUE;
  }
}

static gboolean
log_writer_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogWriter *self = (LogWriter *) s;
  
  if ((self->options->flags & LWOF_NO_STATS) == 0 && !self->dropped_messages)
    stats_register_counter(SC_TYPE_DROPPED, self->options->stats_name, &self->dropped_messages, !!(self->options->flags & LWOF_SHARE_STATS));
  return TRUE;
}

static gboolean
log_writer_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogWriter *self = (LogWriter *) s;

  if (self->source)
    {
      g_source_destroy(self->source);
      g_source_unref(self->source);
      self->source = NULL;
    }

  if (self->dropped_messages)
    stats_orphan_counter(SC_TYPE_DROPPED, self->options->stats_name, &self->dropped_messages);
  
  return TRUE;
}

static void
log_writer_free(LogPipe *s)
{
  LogWriter *self = (LogWriter *) s;
  
  log_queue_free(self->queue);

  g_free(self);
}

gboolean
log_writer_reopen(LogPipe *s, FDWrite *newfd)
{
  LogWriter *self = (LogWriter *) s;
  
  if (self->source)
    {
      g_source_destroy(self->source);
      g_source_unref(self->source);
      self->source = NULL;
    }
  
  if (newfd)
    {
      self->source = log_writer_watch_new(self, newfd);
      g_source_attach(self->source, NULL);
    }
  
  return TRUE;
}

void
log_writer_set_options(LogWriter *self, LogWriterOptions *options)
{
  self->options = options;
}

LogPipe *
log_writer_new(guint32 flags, LogPipe *control, LogWriterOptions *options)
{
  LogWriter *self = g_new0(LogWriter, 1);
  
  log_pipe_init_instance(&self->super);
  self->super.init = log_writer_init;
  self->super.deinit = log_writer_deinit;
  self->super.queue = log_writer_queue;
  self->super.free_fn = log_writer_free;

  self->options = options;  
  self->queue = log_queue_new(options->mem_fifo_size);
  self->flags = flags;
  self->control = control;
  self->throttle_buckets = self->options->throttle;
  return &self->super;
}

void 
log_writer_options_defaults(LogWriterOptions *options)
{
  options->mem_fifo_size = -1;
  options->template = NULL;
  options->use_time_recvd = -1;
  options->flush_lines = -1;
  options->flush_timeout = -1;
  options->ts_format = -1;
  options->zone_offset = -1;
  options->frac_digits = -1;
}

void 
log_writer_options_set_template_escape(LogWriterOptions *options, gboolean enable)
{
  if (options->template && options->template->def_inline)
    {
      log_template_set_escape(options->template, enable);
    }
  else
    {
      msg_error("Macro escaping can only be specified for inline templates", NULL);
    }
}

void
log_writer_options_init(LogWriterOptions *options, GlobalConfig *cfg, guint32 flags, const gchar *stats_name)
{
  LogTemplate *template;

 /* NOTE: free everything that might have remained from a previous init
  * call, this way init can be called any number of times, without calling
  * destroy first. We only need to keep options->template around as that's
  * never initialized based on the configuration
  */
  
  template = log_template_ref(options->template);
  log_writer_options_destroy(options);
  options->template = template;
  options->flags = flags;
  if (options->mem_fifo_size == -1)
    options->mem_fifo_size = MAX(1000, cfg->log_fifo_size);
  if (options->use_time_recvd == -1)
    options->use_time_recvd = cfg->use_time_recvd;
    
  if (options->flush_lines == -1)
    options->flush_lines = cfg->flush_lines;
  if (options->flush_timeout == -1)
    options->flush_timeout = cfg->flush_timeout;
  if (options->frac_digits == -1)
    options->frac_digits = cfg->frac_digits;
    
  if (options->mem_fifo_size < options->flush_lines)
    {
      msg_error("The value of flush_lines must be less than log_fifo_size",
                evt_tag_int("log_fifo_size", options->mem_fifo_size),
                evt_tag_int("flush_lines", options->flush_lines),
                NULL);
      options->flush_lines = options->mem_fifo_size - 1;
    }

  if (options->ts_format == -1)
    options->ts_format = cfg->ts_format;
  if (options->zone_offset == -1)
    options->zone_offset = cfg->send_zone_offset;
  options->file_template = log_template_ref(cfg->file_template);
  options->proto_template = log_template_ref(cfg->proto_template);
  options->stats_name = stats_name ? g_strdup(stats_name) : NULL;
}

void
log_writer_options_destroy(LogWriterOptions *options)
{
  log_template_unref(options->template);
  log_template_unref(options->file_template);
  log_template_unref(options->proto_template);
  if (options->stats_name)
    g_free(options->stats_name);
}
