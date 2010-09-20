/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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
  
#include "logwriter.h"
#include "messages.h"
#include "stats.h"
#include "misc.h"

#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

typedef struct _LogWriterWatch
{
  GSource super;
  GPollFD pollfd;
  LogWriter *writer;
  LogProto *proto;
  GTimeVal flush_target;
  GTimeVal error_suspend_target;
  GTimeVal last_throttle_check;
  gboolean flush_waiting_for_timeout:1,
           input_means_connection_broken:1,
           error_suspend:1;
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

static gboolean log_writer_flush_log(LogWriter *self, LogProto *proto);
static void log_writer_broken(LogWriter *self, gint notify_code);
static gboolean log_writer_throttling(LogWriter *self);


static gboolean
log_writer_fd_prepare(GSource *source,
                      gint *timeout)
{
  LogWriterWatch *self = (LogWriterWatch *) source;
  gint64 num_elements = log_queue_get_length(self->writer->queue);
  GTimeVal now;
  GIOCondition proto_cond;

  self->pollfd.events = G_IO_ERR;
  self->pollfd.revents = 0;

  g_source_get_current_time(source, &now);
  if (log_proto_prepare(self->proto, &self->pollfd.fd, &proto_cond, timeout))
    return TRUE;
  
  /* recalculate buckets */
  
  if (self->writer->options->throttle > 0)
    {
      gint64 diff;
      gint new_buckets;
      
      /* throttling is enabled, calculate new buckets */
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

  if (G_UNLIKELY(self->error_suspend))
    {
      *timeout = g_time_val_diff(&self->error_suspend_target, &now) / 1000;
      if (*timeout <= 0)
        {
          msg_notice("Error suspend timeout has elapsed, attempting to write again",
                     evt_tag_int("fd", log_proto_get_fd(self->proto)),
                     NULL);
          self->error_suspend = FALSE;
          *timeout = -1;
        }
      else
        {
          return FALSE;
        }
    }
  
  if ((self->writer->options->flush_lines == 0 && (!log_writer_throttling(self->writer) && num_elements != 0)) ||
      (self->writer->options->flush_lines > 0  && (!log_writer_throttling(self->writer) && num_elements >= self->writer->options->flush_lines)))
    {
      /* we need to flush our buffers */
      self->pollfd.events |= proto_cond;
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
          glong to = g_time_val_diff(&self->flush_target, &now) / 1000;
          if (to <= 0)
            {
              /* timeout elapsed, start polling again */
              if (self->writer->flags & LW_ALWAYS_WRITABLE)
                return TRUE;
              self->pollfd.events = proto_cond;
            }
          else
            {
              *timeout = to;
            }
        }
      return FALSE;
    }
  else
    {
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
  
  if ((self->pollfd.events & G_IO_OUT) && (self->writer->flags & LW_ALWAYS_WRITABLE))
    {
      self->pollfd.revents = G_IO_OUT;
      return TRUE;
    }
  return FALSE;
}

static gboolean
log_writer_fd_check(GSource *source)
{
  LogWriterWatch *self = (LogWriterWatch *) source;
  gint64 num_elements = log_queue_get_length(self->writer->queue);
  
  if (self->error_suspend)
    return FALSE;
  
  if (num_elements && !log_writer_throttling(self->writer))
    {
      /* we have data to flush */
      if (self->flush_waiting_for_timeout)
        {
          GTimeVal tv;

          /* check if timeout elapsed */
          g_source_get_current_time(source, &tv);
          if (!(self->flush_target.tv_sec <= tv.tv_sec || (self->flush_target.tv_sec == tv.tv_sec && self->flush_target.tv_usec <= tv.tv_usec)))
            return FALSE;
          if ((self->writer->flags & LW_ALWAYS_WRITABLE))
            return TRUE;
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
                evt_tag_int("fd", log_proto_get_fd(self->proto)),
                NULL);
      log_writer_broken(self->writer, NC_CLOSE);
      return FALSE;
    }
  else if (self->pollfd.revents & (G_IO_ERR) && num_elements == 0)
    {
      msg_error("POLLERR occurred while idle",
                evt_tag_int("fd", log_proto_get_fd(self->proto)),
                NULL);
      log_writer_broken(self->writer, NC_WRITE_ERROR);
    }
  else if (num_elements)
    {
      if (!log_writer_flush_log(self->writer, self->proto))
        {
          self->error_suspend = TRUE;
          g_source_get_current_time(source, &self->error_suspend_target);
          g_time_val_add(&self->error_suspend_target, self->writer->options->time_reopen * 1e6);

          log_writer_broken(self->writer, NC_WRITE_ERROR);
          
          if (self->writer->source == (GSource *) self)
            {
              msg_notice("Suspending write operation because of an I/O error",
                         evt_tag_int("fd", log_proto_get_fd(self->proto)),
                         evt_tag_int("time_reopen", self->writer->options->time_reopen),
                         NULL);
            }
          return TRUE;
        }
    }
  return TRUE;
}

static void
log_writer_fd_finalize(GSource *source)
{
  LogWriterWatch *self = (LogWriterWatch *) source;

  log_proto_free(self->proto);
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
log_writer_watch_new(LogWriter *writer, LogProto *proto)
{
  LogWriterWatch *self = (LogWriterWatch *) g_source_new(&log_writer_source_funcs, sizeof(LogWriterWatch));
  
  self->writer = writer;
  self->proto = proto;
  log_pipe_ref(&self->writer->super);
  g_source_set_priority(&self->super, LOG_PRIORITY_WRITER);
  
  if ((writer->flags & LW_ALWAYS_WRITABLE) == 0)
    g_source_add_poll(&self->super, &self->pollfd);
  return &self->super;
}

static gboolean
log_writer_throttling(LogWriter *self)
{
  return self->options->throttle > 0 && self->throttle_buckets == 0;
}

static void
log_writer_last_msg_release(LogWriter *self)
{
  if (self->last_msg_timerid)
    g_source_remove(self->last_msg_timerid);

  if (self->last_msg)
    log_msg_unref(self->last_msg);

  self->last_msg = NULL;
  self->last_msg_count = 0;
  self->last_msg_timerid = 0;
}

static void
log_writer_last_msg_flush(LogWriter *self)
{
  LogMessage *m;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gchar hostname[256];
  gchar buf[1024];
  gssize len;
  const gchar *p;

  msg_debug("Suppress timer elapsed, emitting suppression summary", 
            NULL);

  getlonghostname(hostname, sizeof(hostname));
  m = log_msg_new_empty();
  m->timestamps[LM_TS_STAMP] = m->timestamps[LM_TS_RECVD];
  m->pri = self->last_msg->pri;
  m->flags = LF_INTERNAL | LF_LOCAL;

  p = log_msg_get_value(self->last_msg, LM_V_HOST, &len);
  log_msg_set_value(m, LM_V_HOST, p, len);
  p = log_msg_get_value(self->last_msg, LM_V_PROGRAM, &len);
  log_msg_set_value(m, LM_V_PROGRAM, p, len);

  len = g_snprintf(buf, sizeof(buf), "Last message '%.20s' repeated %d times, supressed by syslog-ng on %s",
                   log_msg_get_value(self->last_msg, LM_V_MESSAGE, NULL),
                   self->last_msg_count,
                   hostname);
  log_msg_set_value(m, LM_V_MESSAGE, buf, len);

  path_options.flow_control = FALSE;
  if (!log_queue_push_tail(self->queue, m, &path_options))
    {
      stats_counter_inc(self->dropped_messages);
      msg_debug("Destination queue full, dropping suppressed message",
                evt_tag_int("queue_len", log_queue_get_length(self->queue)),
                evt_tag_int("mem_fifo_size", self->options->mem_fifo_size),
                NULL);
      log_msg_drop(m, &path_options);
    }

  log_writer_last_msg_release(self);
}

static gboolean
last_msg_timer(gpointer pt)
{
  LogWriter *self = (LogWriter *) pt;

  log_writer_last_msg_flush(self);

  return FALSE;
}

/**
 * Remember the last message for dup detection.
 **/
static void
log_writer_last_msg_record(LogWriter *self, LogMessage *lm)
{
  if (self->last_msg)
    log_msg_unref(self->last_msg);

  log_msg_ref(lm);
  self->last_msg = lm;
  self->last_msg_count = 0;
}

/**
 * log_writer_last_msg_check:
 *
 * This function is called to suppress duplicate messages from a given host.
 *
 * Returns TRUE to indicate that the message was consumed.
 **/
static gboolean
log_writer_last_msg_check(LogWriter *self, LogMessage *lm, const LogPathOptions *path_options)
{
  if (self->last_msg)
    {
      if (self->last_msg->timestamps[LM_TS_RECVD].time.tv_sec >= lm->timestamps[LM_TS_RECVD].time.tv_sec - self->options->suppress &&
          strcmp(log_msg_get_value(self->last_msg, LM_V_MESSAGE, NULL), log_msg_get_value(lm, LM_V_MESSAGE, NULL)) == 0 &&
          strcmp(log_msg_get_value(self->last_msg, LM_V_HOST, NULL), log_msg_get_value(lm, LM_V_HOST, NULL)) == 0 &&
          strcmp(log_msg_get_value(self->last_msg, LM_V_PROGRAM, NULL), log_msg_get_value(lm, LM_V_PROGRAM, NULL)) == 0 &&
          strcmp(log_msg_get_value(self->last_msg, LM_V_PID, NULL), log_msg_get_value(lm, LM_V_PID, NULL)) == 0 &&
          strcmp(log_msg_get_value(lm, LM_V_MESSAGE, NULL), "-- MARK --") != 0)
        {
          stats_counter_inc(self->suppressed_messages);
          msg_debug("Suppressing duplicate message",
                    evt_tag_str("host", log_msg_get_value(lm, LM_V_HOST, NULL)),
                    evt_tag_str("msg", log_msg_get_value(lm, LM_V_MESSAGE, NULL)),
                    NULL);
          self->last_msg_count++;
          
          if (self->last_msg_count == 1)
            {
              /* we only create the timer if there's at least one suppressed message */
              self->last_msg_timerid = g_timeout_add(self->options->suppress * 1000, last_msg_timer, self);
            }
          log_msg_drop(lm, path_options);
          return TRUE;
        }

      if (self->last_msg_count)
        log_writer_last_msg_flush(self);
      else
        log_writer_last_msg_release(self);
    }

  log_writer_last_msg_record(self, lm);

  return FALSE;
}

static void
log_writer_queue(LogPipe *s, LogMessage *lm, const LogPathOptions *path_options)
{
  LogWriter *self = (LogWriter *) s;

  if (self->options->suppress > 0 && log_writer_last_msg_check(self, lm, path_options))
    return;
  
  stats_counter_inc(self->processed_messages);
  if (!log_queue_push_tail(self->queue, lm, path_options))
    {
      /* drop incoming message, we must ack here, otherwise the sender might
       * block forever, however this should not happen unless the sum of
       * window_sizes of sources feeding this writer exceeds log_fifo_size
       * or if flow control is not turned on.
       */
      
      /* we don't send a message here since the system is draining anyway */
      
      stats_counter_inc(self->dropped_messages);
      msg_debug("Destination queue full, dropping message",
                evt_tag_int("queue_len", log_queue_get_length(self->queue)),
                evt_tag_int("mem_fifo_size", self->options->mem_fifo_size),
                NULL);
      log_msg_drop(lm, path_options);
      return;
    }
}

static void
log_writer_append_value(GString *result, LogMessage *lm, NVHandle handle, gboolean use_nil, gboolean append_space)
{
  const gchar *value;
  gssize value_len;

  value = log_msg_get_value(lm, handle, &value_len);
  if (use_nil && value_len == 0)
    g_string_append_c(result, '-');
  else
    {
      gchar *space;
      
      space = strchr(value, ' ');
      
      if (!space)
        g_string_append_len(result, value, value_len);
      else
        g_string_append_len(result, value, space - value);
    }
  if (append_space)
    g_string_append_c(result, ' ');
}

void
log_writer_format_log(LogWriter *self, LogMessage *lm, GString *result)
{
  LogTemplate *template = NULL;
  LogStamp *stamp;
  guint32 seq_num;
  static NVHandle meta_seqid = 0;

  if (!meta_seqid)
    meta_seqid = log_msg_get_value_handle(".SDATA.meta.sequenceId");

  if (lm->flags & LF_LOCAL)
    {
      seq_num = self->seq_num;
    }
  else
    {
      const gchar *seqid;
      gssize seqid_length;

      seqid = log_msg_get_value(lm, meta_seqid, &seqid_length);
      seqid = APPEND_ZERO(seqid, seqid_length);
      if (seqid[0])
        seq_num = strtol(seqid, NULL, 10);
      else
        seq_num = 0;
    }
  
  /* no template was specified, use default */
  stamp = &lm->timestamps[LM_TS_STAMP];

  if ((self->flags & LW_SYSLOG_PROTOCOL) || (self->options->options & LWO_SYSLOG_PROTOCOL))
    {
      gint len;
       
      /* we currently hard-wire version 1 */
      g_string_sprintf(result, "<%d>%d ", lm->pri, 1);
 
      log_stamp_append_format(stamp, result, TS_FMT_ISO, 
                              time_zone_info_get_offset(self->options->template_options.time_zone_info[LTZ_SEND], stamp->time.tv_sec),
                              self->options->template_options.frac_digits);
      g_string_append_c(result, ' ');
      
      log_writer_append_value(result, lm, LM_V_HOST, TRUE, TRUE);
      log_writer_append_value(result, lm, LM_V_PROGRAM, TRUE, TRUE);
      log_writer_append_value(result, lm, LM_V_PID, TRUE, TRUE);
      log_writer_append_value(result, lm, LM_V_MSGID, TRUE, TRUE);

#if 0
      if (lm->flags & LF_LOCAL)
        {
          gchar sequence_id[16];
          
          g_snprintf(sequence_id, sizeof(sequence_id), "%d", seq_num);
          log_msg_update_sdata(lm, "meta", "sequenceId", sequence_id);
        }
#endif
      len = result->len;
      log_msg_append_format_sdata(lm, result);
      if (len == result->len)
        {
          /* NOTE: sd_param format did not generate any output, take it as an empty SD string */
          g_string_append_c(result, '-');
        }
       
      if (self->options->template)
        {
          g_string_append_c(result, ' ');
          if (lm->flags & LF_UTF8)
            g_string_append_len(result, "\xEF\xBB\xBF", 3);
          log_template_append_format(self->options->template, lm, 
                                     &self->options->template_options,
                                     LTZ_SEND,
                                     seq_num,
                                     result);
        }
      else
        {
          const gchar *p;
          gssize len;

          p = log_msg_get_value(lm, LM_V_MESSAGE, &len);
          g_string_append_c(result, ' ');
          if (len != 0)
            {
              if (lm->flags & LF_UTF8)
                g_string_append_len(result, "\xEF\xBB\xBF", 3);

              g_string_append_len(result, p, len);
            }
        }
      g_string_append_c(result, '\n');
    }
  else
    {

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
                              &self->options->template_options,
                              LTZ_SEND,
                              seq_num,
                              result);

        }
      else 
        {
          const gchar *p;
          gssize len;

          if (self->flags & LW_FORMAT_FILE)
            {
              log_stamp_format(stamp, result, self->options->template_options.ts_format,
                               time_zone_info_get_offset(self->options->template_options.time_zone_info[LTZ_SEND], stamp->time.tv_sec),
                               self->options->template_options.frac_digits);
            }
          else if (self->flags & LW_FORMAT_PROTO)
            {
              g_string_sprintf(result, "<%d>", lm->pri);

              /* always use BSD timestamp by default, the use can override this using a custom template */
              log_stamp_append_format(stamp, result, TS_FMT_BSD,
                                      time_zone_info_get_offset(self->options->template_options.time_zone_info[LTZ_SEND], stamp->time.tv_sec),
                                      self->options->template_options.frac_digits);
            }
          g_string_append_c(result, ' ');

          p = log_msg_get_value(lm, LM_V_HOST, &len);
          g_string_append_len(result, p, len);
          g_string_append_c(result, ' ');

          if ((lm->flags & LF_LEGACY_MSGHDR))
            {
              p = log_msg_get_value(lm, LM_V_LEGACY_MSGHDR, &len);
              g_string_append_len(result, p, len);
            }
          else
            {
              p = log_msg_get_value(lm, LM_V_PROGRAM, &len);
              if (len > 0)
                {
                  g_string_append_len(result, p, len);
                  p = log_msg_get_value(lm, LM_V_PID, &len);
                  if (len > 0)
                    {
                      g_string_append_c(result, '[');
                      g_string_append_len(result, p, len);
                      g_string_append_c(result, ']');
                    }
                  g_string_append_len(result, ": ", 2);
                }
            }
          p = log_msg_get_value(lm, LM_V_MESSAGE, &len);
          g_string_append_len(result, p, len);
          g_string_append_c(result, '\n');

        }
    }
  if (self->options->options & LWO_NO_MULTI_LINE)
    {
      gchar *p;

      p = result->str;
      /* NOTE: the size is calculated to leave trailing new line */
      while ((p = find_cr_or_lf(p, result->str + result->len - p - 1)))
        {
          *p = ' ';
          p++;
        }

    }
}

static void
log_writer_broken(LogWriter *self, gint notify_code)
{
  log_pipe_notify(self->control, &self->super, notify_code, self);
}

static gboolean 
log_writer_flush_log(LogWriter *self, LogProto *proto)
{
  GString *line = NULL;
  gint64 num_elements;
  
  line = g_string_sized_new(128);
  num_elements = log_queue_get_length(self->queue);
  while (num_elements > 0 && !log_writer_throttling(self))
    {
      LogMessage *lm;
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
      gboolean consumed = FALSE;
      
      if (!log_queue_pop_head(self->queue, &lm, &path_options, FALSE))
        g_assert_not_reached();

      msg_set_context(lm);

      log_writer_format_log(self, lm, line);
      
      /* account this message against the throttle rate */
      self->throttle_buckets--;
      
      if (line->len)
        {
          LogProtoStatus status;

          status = log_proto_post(proto, (guchar *) line->str, line->len, &consumed);
          if (status == LPS_ERROR)
            {
              g_string_free(line, TRUE);
              return FALSE;
            }
          if (consumed)
            {
              line->str = g_malloc0(1);
              line->allocated_len = 1;
              line->len = 0;
            }
        }
      if (consumed)
        {
          if (lm->flags & LF_LOCAL)
            step_sequence_number(&self->seq_num);
          log_msg_ack(lm, &path_options);
          log_msg_unref(lm);
        }
      else
        {
          /* push back to the queue */
          log_queue_push_head(self->queue, lm, &path_options);

          /* force exit of the loop */
          num_elements = 1;
        }
        
      msg_set_context(NULL);
      num_elements--;
    }
  g_string_free(line, TRUE);
  return TRUE;
}

static gboolean
log_writer_init(LogPipe *s)
{
  LogWriter *self = (LogWriter *) s;

  if (!self->queue)
    self->queue = log_queue_new(self->options->mem_fifo_size);
  
  if ((self->options->options & LWO_NO_STATS) == 0 && !self->dropped_messages)
    {
      stats_register_counter(self->stats_level, self->stats_source | SCS_DESTINATION, self->stats_id, self->stats_instance, SC_TYPE_DROPPED, &self->dropped_messages);
      if (self->options->suppress > 0)
        stats_register_counter(self->stats_level, self->stats_source | SCS_DESTINATION, self->stats_id, self->stats_instance, SC_TYPE_SUPPRESSED, &self->suppressed_messages);
      stats_register_counter(self->stats_level, self->stats_source | SCS_DESTINATION, self->stats_id, self->stats_instance, SC_TYPE_PROCESSED, &self->processed_messages);
      
      stats_register_counter(self->stats_level, self->stats_source | SCS_DESTINATION, self->stats_id, self->stats_instance, SC_TYPE_STORED, &self->stored_messages);
    }
  return TRUE;
}

static gboolean
log_writer_deinit(LogPipe *s)
{
  LogWriter *self = (LogWriter *) s;

  if (self->source)
    {
      g_source_destroy(self->source);
      g_source_unref(self->source);
      self->source = NULL;
    }

  stats_unregister_counter(self->stats_source | SCS_DESTINATION, self->stats_id, self->stats_instance, SC_TYPE_DROPPED, &self->dropped_messages);
  stats_unregister_counter(self->stats_source | SCS_DESTINATION, self->stats_id, self->stats_instance, SC_TYPE_SUPPRESSED, &self->suppressed_messages);
  stats_unregister_counter(self->stats_source | SCS_DESTINATION, self->stats_id, self->stats_instance, SC_TYPE_PROCESSED, &self->processed_messages);
  stats_unregister_counter(self->stats_source | SCS_DESTINATION, self->stats_id, self->stats_instance, SC_TYPE_STORED, &self->stored_messages);
  
  return TRUE;
}

static void
log_writer_free(LogPipe *s)
{
  LogWriter *self = (LogWriter *) s;
  
  if (self->queue)
    log_queue_free(self->queue);
  log_writer_last_msg_release(self);
  g_free(self->stats_id);
  g_free(self->stats_instance);
  log_pipe_free(s);
}

gboolean
log_writer_reopen(LogPipe *s, LogProto *proto)
{
  LogWriter *self = (LogWriter *) s;
  
  /* old fd is freed by the source */
  if (self->source)
    {
      g_source_destroy(self->source);
      g_source_unref(self->source);
      self->source = NULL;
    }
  
  if (proto)
    {
      self->source = log_writer_watch_new(self, proto);
      g_source_attach(self->source, NULL);
    }
  init_sequence_number(&self->seq_num);
  
  return TRUE;
}

void
log_writer_set_options(LogWriter *self, LogPipe *control, LogWriterOptions *options, gint stats_level, gint stats_source, const gchar *stats_id, const gchar *stats_instance)
{
  self->control = control;
  self->options = options;

  self->stats_level = stats_level;
  self->stats_source = stats_source;
  self->stats_id = stats_id ? g_strdup(stats_id) : NULL;
  self->stats_instance = stats_instance ? g_strdup(stats_instance) : NULL;

  self->throttle_buckets = self->options->throttle;
}

LogPipe *
log_writer_new(guint32 flags)
{
  LogWriter *self = g_new0(LogWriter, 1);
  
  log_pipe_init_instance(&self->super);
  self->super.init = log_writer_init;
  self->super.deinit = log_writer_deinit;
  self->super.queue = log_writer_queue;
  self->super.free_fn = log_writer_free;

  self->flags = flags;
  self->last_msg = NULL;
  self->last_msg_count = 0;
  self->last_msg_timerid = 0;
  return &self->super;
}

void 
log_writer_options_defaults(LogWriterOptions *options)
{
  options->mem_fifo_size = -1;
  options->template = NULL;
  options->flush_lines = -1;
  options->flush_timeout = -1;
  log_template_options_defaults(&options->template_options);
  options->time_reopen = -1;
  options->suppress = 0;
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


/*
 * NOTE: options_init and options_destroy are a bit weird, because their
 * invocation is not completely symmetric:
 *
 *   - init is called from driver init (e.g. affile_dd_init), 
 *   - destroy is called from driver free method (e.g. affile_sd_free, NOT affile_dd_deinit)
 *
 * The reason:
 *   - when initializing the reloaded configuration fails for some reason,
 *     we have to fall back to the old configuration, thus we cannot dump
 *     the information stored in the Options structure.
 *
 * For the reasons above, init and destroy behave the following way:
 *
 *   - init is idempotent, it can be called multiple times without leaking
 *     memory, and without loss of information
 *   - destroy is only called once, when the options are indeed to be destroyed
 *
 * As init allocates memory, it has to take care about freeing memory
 * allocated by the previous init call (or it has to reuse those).
 *   
 */
void
log_writer_options_init(LogWriterOptions *options, GlobalConfig *cfg, guint32 option_flags)
{
  LogTemplate *template;
  gchar *time_zone[2];
  TimeZoneInfo *time_zone_info[2];
  gint i;

  template = log_template_ref(options->template);

  for (i = 0; i < LTZ_MAX; i++)
    {
      time_zone[i] = options->template_options.time_zone[i];
      time_zone_info[i] = options->template_options.time_zone_info[i];
      options->template_options.time_zone[i] = NULL;
      options->template_options.time_zone_info[i] = NULL;
    }

  log_writer_options_destroy(options);
  log_template_options_destroy(&options->template_options);
  
  /* restroe the config */
  options->template = template;
  for (i = 0; i < LTZ_MAX; i++)
    {
      options->template_options.time_zone[i] = time_zone[i];
      options->template_options.time_zone_info[i] = time_zone_info[i];
    }
  log_template_options_init(&options->template_options, cfg);
  options->options |= option_flags;
  if (options->mem_fifo_size == -1)
    options->mem_fifo_size = MAX(1000, cfg->log_fifo_size);
    
  if (options->flush_lines == -1)
    options->flush_lines = cfg->flush_lines;
  if (options->flush_timeout == -1)
    options->flush_timeout = cfg->flush_timeout;
    
  if (options->mem_fifo_size < options->flush_lines)
    {
      msg_error("The value of flush_lines must be less than log_fifo_size",
                evt_tag_int("log_fifo_size", options->mem_fifo_size),
                evt_tag_int("flush_lines", options->flush_lines),
                NULL);
      options->flush_lines = options->mem_fifo_size - 1;
    }

  if (options->time_reopen == -1)
    options->time_reopen = cfg->time_reopen;
  options->file_template = log_template_ref(cfg->file_template);
  options->proto_template = log_template_ref(cfg->proto_template);
}

void
log_writer_options_destroy(LogWriterOptions *options)
{
  log_template_options_destroy(&options->template_options);
  log_template_unref(options->template);
  log_template_unref(options->file_template);
  log_template_unref(options->proto_template);
}

gint
log_writer_options_lookup_flag(const gchar *flag)
{
  if (strcmp(flag, "syslog_protocol") == 0 || strcmp(flag, "syslog-protocol") == 0)
    return LWO_SYSLOG_PROTOCOL;
  if (strcmp(flag, "no-multi-line") == 0 || strcmp(flag, "no_multi_line") == 0)
    return LWO_NO_MULTI_LINE;
  msg_error("Unknown dest writer flag", evt_tag_str("flag", flag), NULL);
  return 0;
}
