#include "logwriter.h"
#include "messages.h"
#include "macros.h"
#include "fdwrite.h"

#include <unistd.h>
#include <assert.h>

typedef struct _LogWriterWatch
{
  GSource super;
  GPollFD pollfd;
  LogWriter *writer;
  FDWrite *fd;
} LogWriterWatch;

static gboolean log_writer_flush_log(LogWriter *self, FDWrite *fd);
static void log_writer_broken(LogWriter *self, FDWrite *fd);

static gboolean
log_writer_fd_prepare(GSource *source,
                      gint *timeout)
{
  LogWriterWatch *self = (LogWriterWatch *) source;

  if (self->writer->queue->length || self->writer->partial)
    self->pollfd.events = self->fd->cond;
  else if (self->writer->flags & LW_DETECT_EOF)
    self->pollfd.events = G_IO_HUP;
  else
    self->pollfd.events = 0;
  self->pollfd.revents = 0;
  return FALSE;
}

static gboolean
log_writer_fd_check(GSource *source)
{
  LogWriterWatch *self = (LogWriterWatch *) source;
  
  return !!(self->pollfd.revents & (G_IO_OUT | G_IO_ERR | G_IO_HUP | G_IO_IN));
}

static gboolean
log_writer_fd_dispatch(GSource *source,
		       GSourceFunc callback,
		       gpointer user_data)
{
  LogWriterWatch *self = (LogWriterWatch *) source;
  if (self->pollfd.revents & G_IO_HUP)
    {
      log_writer_broken(self->writer, self->fd);
      return FALSE;
    }
  else if (self->pollfd.revents & self->fd->cond)
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

static void
log_writer_queue(LogPipe *s, LogMessage *lm, gint path_flags)
{
  LogWriter *self = (LogWriter *) s;
  
  if ((self->queue->length / 2) == self->options->fifo_size)
    {
      /* drop incoming message, we must ack here, otherwise the sender might
       * block forever, however this should not happen unless the sum of
       * window_sizes of sources feeding this writer exceeds log_fifo_size
       * or if flow control is not turned on.
       */
      
      /* we don't send a message here since the system is draining anyway */
      
      /* self->options->fifo_full_drop++; */
      msg_debug("Destination queue full, dropping message",
                evt_tag_int("queue_len", self->queue->length/2),
                evt_tag_int("fifo_size", self->options->fifo_size),
                NULL);
      log_msg_ack(lm);
      return;
    }
  g_queue_push_tail(self->queue, lm);
  g_queue_push_tail(self->queue, (gpointer) (0x80000000 | path_flags));
}

static void
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
		          ((self->options->options & LWO_TMPL_TIME_RECVD) ? MF_STAMP_RECVD : 0), 
		          result);

    }
  else 
    {
      GString *ts;
      gboolean ts_free;
      
      if (!self->options->keep_timestamp || !lm->date->len) 
        {
          /* reformat time stamp */
          LogStamp *stamp;
          
          if (self->options->use_time_recvd)
            stamp = &lm->recvd;
          else
            stamp = &lm->stamp;
          ts_free = TRUE;
          ts = g_string_sized_new(64);
          log_stamp_format(stamp, ts, self->options->ts_format, self->options->tz_convert);
        }
      else
        {
          ts_free = FALSE;
          ts = lm->date;
        }
      if (self->flags & LW_FORMAT_FILE)
        {
          g_string_sprintf(result, "%s %s %s\n", ts->str, lm->host->str, lm->msg->str);
      
        }
      else if (self->flags & LW_FORMAT_PROTO)
        {
          g_string_sprintf(result, "<%d>%s %s %s\n", lm->pri, ts->str, lm->host->str, lm->msg->str);
        }
      if (ts_free)
        g_string_free(ts, TRUE);
    }
}

static void
log_writer_broken(LogWriter *self, FDWrite *fd)
{
  /* the connection seems to be broken */
  msg_error("EOF occurred while idle",
            evt_tag_int("fd", fd->fd),
            NULL);
  log_pipe_deinit(&self->super, NULL, NULL);
  log_pipe_notify(self->control, &self->super, NC_CLOSE, self);
}

static gboolean 
log_writer_flush_log(LogWriter *self, FDWrite *fd)
{
  GString *line = NULL;
  gint rc;
  
  if (self->partial)
    {
      int len = self->partial->len - self->partial_pos;
      
      rc = fd_write(fd, &self->partial->str[self->partial_pos], len);
      if (rc == -1)
        {
          goto write_error;
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
  while (!g_queue_is_empty(self->queue))
    {
      LogMessage *lm;
      gint path_flags;
      
      lm = g_queue_pop_head(self->queue);
      path_flags = (gint) (g_queue_pop_head(self->queue)) & 0x7FFFFFFF;
      
      log_writer_format_log(self, lm, line);
      
      if ((path_flags & PF_FLOW_CTL_OFF) == 0)
        {
          log_msg_ack(lm);
        }
      log_msg_unref(lm);
      
      if (line->len)
        {
          rc = fd_write(fd, line->str, line->len);
          
          if (rc == -1)
            {
              self->partial = line;
              self->partial_pos = 0;
              line = NULL;
              goto write_error;
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
        log_pipe_deinit(&self->super, NULL, NULL);
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
  return TRUE;
}

static gboolean
log_writer_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogWriter *self = (LogWriter *) s;

  if (self->source)
    g_source_destroy(self->source);
  return TRUE;
}

static void
log_writer_free(LogPipe *s)
{
  LogWriter *self = (LogWriter *) s;
  
  g_queue_free(self->queue);
  log_pipe_unref(self->control);
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
  self->queue = g_queue_new();
  self->flags = flags;
  self->control = control;
  log_pipe_ref(control);
  return &self->super;
}

void 
log_writer_options_defaults(LogWriterOptions *options)
{
  options->fifo_size = -1;
  options->template = NULL;
  options->keep_timestamp = -1;
  options->use_time_recvd = -1;
  options->tz_convert = -1;
  options->ts_format = -1;
}

void
log_writer_options_init(LogWriterOptions *options, GlobalConfig *cfg, gboolean fixed_stamp)
{
  if (options->fifo_size == -1)
    options->fifo_size = cfg->log_fifo_size;
  if (options->use_time_recvd == -1)
    options->use_time_recvd = cfg->use_time_recvd;

  if (!fixed_stamp)
    {
      if (options->keep_timestamp == -1)
        options->keep_timestamp = cfg->keep_timestamp;
      if (options->tz_convert == -1)
        options->tz_convert = cfg->tz_convert;
      if (options->ts_format == -1)
        options->ts_format = cfg->ts_format;
    }
  else
    {
      options->keep_timestamp = FALSE;
      options->tz_convert = TZ_CNV_LOCAL;
      options->ts_format = TS_FMT_BSD;
    }
  options->file_template = log_template_ref(cfg->file_template);
  options->proto_template = log_template_ref(cfg->proto_template);
}

void
log_writer_options_destroy(LogWriterOptions *options)
{
  log_template_unref(options->template);
  log_template_unref(options->file_template);
  log_template_unref(options->proto_template);
}
