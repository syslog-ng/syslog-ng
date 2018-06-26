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

#include "logsource.h"
#include "messages.h"
#include "host-resolve.h"
#include "timeutils.h"
#include "stats/stats-registry.h"
#include "stats/stats-syslog.h"
#include "logmsg/tags.h"
#include "ack_tracker.h"

#include <string.h>

gboolean accurate_nanosleep = FALSE;

void
log_source_wakeup(LogSource *self)
{
  if (self->wakeup)
    self->wakeup(self);

  msg_debug("Source has been resumed", log_pipe_location_tag(&self->super));
}

void
log_source_window_empty(LogSource *self)
{
  if (self->window_empty_cb)
    self->window_empty_cb(self);
  msg_debug("LogSource window is empty");
}

static inline void
_flow_control_window_size_adjust(LogSource *self, guint32 window_size_increment)
{
  guint32 old_window_size;

  window_size_increment += g_atomic_counter_get(&self->suspended_window_size);
  old_window_size = g_atomic_counter_exchange_and_add(&self->window_size, window_size_increment);
  g_atomic_counter_set(&self->suspended_window_size, 0);

  if (old_window_size == 0)
    log_source_wakeup(self);
  if (g_atomic_counter_get(&self->window_size) == self->options->init_window_size)
    log_source_window_empty(self);
}

static void
_flow_control_rate_adjust(LogSource *self)
{
#ifdef SYSLOG_NG_HAVE_CLOCK_GETTIME
  guint32 cur_ack_count, last_ack_count;
  /* NOTE: this is racy. msg_ack may be executing in different writer
   * threads. I don't want to lock, all we need is an approximate value of
   * the ACK rate of the last couple of seconds.  */

  if (accurate_nanosleep && self->threaded)
    {
      cur_ack_count = ++self->ack_count;
      if ((cur_ack_count & 0x3FFF) == 0)
        {
          struct timespec now;
          glong diff;

          /* do this every once in a while, once in 16k messages should be fine */

          last_ack_count = self->last_ack_count;

          /* make sure that we have at least 16k messages to measure the rate
           * for.  Because of the race we may have last_ack_count ==
           * cur_ack_count if another thread already measured the same span */

          if (last_ack_count < cur_ack_count - 16383)
            {
              clock_gettime(CLOCK_MONOTONIC, &now);
              if (now.tv_sec > self->last_ack_rate_time.tv_sec + 6)
                {
                  /* last check was too far apart, this means the rate is quite slow. turn off sleeping. */
                  self->window_full_sleep_nsec = 0;
                  self->last_ack_rate_time = now;
                }
              else
                {
                  /* ok, we seem to have a close enough measurement, this means
                   * we do have a high rate.  Calculate how much we should sleep
                   * in case the window gets full */

                  diff = timespec_diff_nsec(&now, &self->last_ack_rate_time);
                  self->window_full_sleep_nsec = (diff / (cur_ack_count - last_ack_count));
                  if (self->window_full_sleep_nsec > 1e6)
                    {
                      /* in case we'd be waiting for 1msec for another free slot in the window, let's go to background instead */
                      self->window_full_sleep_nsec = 0;
                    }
                  else
                    {
                      /* otherwise let's wait for about 8 message to be emptied before going back to the loop, but clamp the maximum time to 0.1msec */
                      self->window_full_sleep_nsec <<= 3;
                      if (self->window_full_sleep_nsec > 1e5)
                        self->window_full_sleep_nsec = 1e5;
                    }
                  self->last_ack_count = cur_ack_count;
                  self->last_ack_rate_time = now;
                }
            }
        }
    }
#endif
}

void
log_source_flow_control_adjust(LogSource *self, guint32 window_size_increment)
{
  _flow_control_window_size_adjust(self, window_size_increment);
  _flow_control_rate_adjust(self);
}

/**
 * log_source_msg_ack:
 *
 * This is running in the same thread as the _destination_, thus care must
 * be taken when manipulating the LogSource data structure.
 **/
static void
log_source_msg_ack(LogMessage *msg, AckType ack_type)
{
  AckTracker *ack_tracker = msg->ack_record->tracker;
  ack_tracker_manage_msg_ack(ack_tracker, msg, ack_type);
}

void
log_source_flow_control_suspend(LogSource *self)
{
  msg_debug("Source has been suspended", log_pipe_location_tag(&self->super));

  g_atomic_counter_set(&self->suspended_window_size, g_atomic_counter_get(&self->window_size));
  g_atomic_counter_set(&self->window_size, 0);
  _flow_control_rate_adjust(self);
}

void
log_source_mangle_hostname(LogSource *self, LogMessage *msg)
{
  const gchar *resolved_name;
  gsize resolved_name_len;
  const gchar *orig_host;

  resolved_name = resolve_sockaddr_to_hostname(&resolved_name_len, msg->saddr, &self->options->host_resolve_options);
  log_msg_set_value(msg, LM_V_HOST_FROM, resolved_name, resolved_name_len);

  orig_host = log_msg_get_value(msg, LM_V_HOST, NULL);
  if (!self->options->keep_hostname || !orig_host || !orig_host[0])
    {
      gchar host[256];
      gint host_len = -1;
      if (G_UNLIKELY(self->options->chain_hostnames))
        {
          msg->flags |= LF_CHAINED_HOSTNAME;
          if (msg->flags & LF_SIMPLE_HOSTNAME)
            {
              /* local without group name */
              host_len = g_snprintf(host, sizeof(host), "%s", resolved_name);
            }
          else if (msg->flags & LF_LOCAL)
            {
              /* local */
              host_len = g_snprintf(host, sizeof(host), "%s@%s", self->options->group_name, resolved_name);
            }
          else if (!orig_host || !orig_host[0])
            {
              /* remote && no hostname */
              host_len = g_snprintf(host, sizeof(host), "%s/%s", resolved_name, resolved_name);
            }
          else
            {
              /* everything else, append source hostname */
              if (orig_host && orig_host[0])
                host_len = g_snprintf(host, sizeof(host), "%s/%s", orig_host, resolved_name);
              else
                {
                  strncpy(host, resolved_name, sizeof(host));
                  /* just in case it is not zero terminated */
                  host[255] = 0;
                }
            }
          if (host_len >= sizeof(host))
            host_len = sizeof(host) - 1;
          log_msg_set_value(msg, LM_V_HOST, host, host_len);
        }
      else
        {
          log_msg_set_value(msg, LM_V_HOST, resolved_name, resolved_name_len);
        }
    }
}

gboolean
log_source_init(LogPipe *s)
{
  LogSource *self = (LogSource *) s;

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, self->options->stats_source | SCS_SOURCE, self->stats_id, self->stats_instance);
  stats_register_counter(self->options->stats_level, &sc_key,
                         SC_TYPE_PROCESSED, &self->recvd_messages);
  stats_register_counter(self->options->stats_level, &sc_key, SC_TYPE_STAMP, &self->last_message_seen);
  stats_unlock();
  return TRUE;
}

gboolean
log_source_deinit(LogPipe *s)
{
  LogSource *self = (LogSource *) s;

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, self->options->stats_source | SCS_SOURCE, self->stats_id, self->stats_instance);
  stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &self->recvd_messages);
  stats_unregister_counter(&sc_key, SC_TYPE_STAMP, &self->last_message_seen);
  stats_unlock();
  return TRUE;
}

void
log_source_post(LogSource *self, LogMessage *msg)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gint old_window_size;

  ack_tracker_track_msg(self->ack_tracker, msg);

  /* NOTE: we start by enabling flow-control, thus we need an acknowledgement */
  path_options.ack_needed = TRUE;
  log_msg_ref(msg);
  log_msg_add_ack(msg, &path_options);
  msg->ack_func = log_source_msg_ack;

  old_window_size = g_atomic_counter_exchange_and_add(&self->window_size, -1);

  if (G_UNLIKELY(old_window_size == 1))
    msg_debug("Source has been suspended", log_pipe_location_tag(&self->super));

  /*
   * NOTE: this assertion validates that the source is not overflowing its
   * own flow-control window size, decreased above, by the atomic statement.
   *
   * If the _old_ value is zero, that means that the decrement operation
   * above has decreased the value to -1.
   */

  g_assert(old_window_size > 0);
  log_pipe_queue(&self->super, msg, &path_options);
}

static gboolean
_invoke_mangle_callbacks(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogSource *self = (LogSource *) s;
  GList *next_item = g_list_first(self->options->source_queue_callbacks);

  while(next_item)
    {
      if(next_item->data)
        {
          if(!((mangle_callback) (next_item->data))(log_pipe_get_config(s),msg,self))
            {
              log_msg_drop(msg, path_options, AT_PROCESSED);
              return FALSE;
            }
        }
      next_item = next_item->next;
    }
  return TRUE;
}

static inline void
_increment_dynamic_stats_counters(const gchar *source_id, const LogMessage *msg)
{
  if (stats_check_level(2))
    {
      stats_lock();

      StatsClusterKey sc_key;
      stats_cluster_logpipe_key_set(&sc_key, SCS_HOST | SCS_SOURCE, NULL, log_msg_get_value(msg, LM_V_HOST, NULL) );
      stats_register_and_increment_dynamic_counter(2, &sc_key, msg->timestamps[LM_TS_RECVD].tv_sec);

      if (stats_check_level(3))
        {
          stats_cluster_logpipe_key_set(&sc_key, SCS_SENDER | SCS_SOURCE, NULL, log_msg_get_value(msg, LM_V_HOST_FROM, NULL) );
          stats_register_and_increment_dynamic_counter(3, &sc_key, msg->timestamps[LM_TS_RECVD].tv_sec);
          stats_cluster_logpipe_key_set(&sc_key, SCS_PROGRAM | SCS_SOURCE, NULL, log_msg_get_value(msg, LM_V_PROGRAM, NULL) );
          stats_register_and_increment_dynamic_counter(3, &sc_key, msg->timestamps[LM_TS_RECVD].tv_sec);

          stats_cluster_logpipe_key_set(&sc_key, SCS_HOST | SCS_SOURCE, source_id, log_msg_get_value(msg, LM_V_HOST, NULL));
          stats_register_and_increment_dynamic_counter(3, &sc_key, msg->timestamps[LM_TS_RECVD].tv_sec);
          stats_cluster_logpipe_key_set(&sc_key, SCS_SENDER | SCS_SOURCE, source_id, log_msg_get_value(msg, LM_V_HOST_FROM,
                                        NULL));
          stats_register_and_increment_dynamic_counter(3, &sc_key, msg->timestamps[LM_TS_RECVD].tv_sec);
        }

      stats_unlock();
    }
}

static void
log_source_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogSource *self = (LogSource *) s;
  gint i;

  msg_debug(">>>>>> Source side message processing begin",
            evt_tag_str("instance", self->stats_instance ? self->stats_instance : "internal"),
            log_pipe_location_tag(s),
            evt_tag_printf("msg", "%p", msg));

  msg_set_context(msg);

  if (!self->options->keep_timestamp)
    msg->timestamps[LM_TS_STAMP] = msg->timestamps[LM_TS_RECVD];

  g_assert(msg->timestamps[LM_TS_STAMP].zone_offset != -1);

  /* $HOST setup */
  log_source_mangle_hostname(self, msg);

  /* $PROGRAM override */
  if (self->options->program_override)
    {
      if (self->options->program_override_len < 0)
        self->options->program_override_len = strlen(self->options->program_override);
      log_msg_set_value(msg, LM_V_PROGRAM, self->options->program_override, self->options->program_override_len);
    }

  /* $HOST override */
  if (self->options->host_override)
    {
      if (self->options->host_override_len < 0)
        self->options->host_override_len = strlen(self->options->host_override);
      log_msg_set_value(msg, LM_V_HOST, self->options->host_override, self->options->host_override_len);
    }

  /* source specific tags */
  if (self->options->tags)
    {
      for (i = 0; i < self->options->tags->len; i++)
        {
          log_msg_set_tag_by_id(msg, g_array_index(self->options->tags, LogTagId, i));
        }
    }

  log_msg_set_tag_by_id(msg, self->options->source_group_tag);

  _increment_dynamic_stats_counters(self->stats_id, msg);
  stats_syslog_process_message_pri(msg->pri);

  /* message setup finished, send it out */

  if (!_invoke_mangle_callbacks(s, msg, path_options))
    return;

  stats_counter_inc(self->recvd_messages);
  stats_counter_set(self->last_message_seen, msg->timestamps[LM_TS_RECVD].tv_sec);
  log_pipe_forward_msg(s, msg, path_options);

  msg_set_context(NULL);

  if (accurate_nanosleep && self->threaded && self->window_full_sleep_nsec > 0 && !log_source_free_to_send(self))
    {
      struct timespec ts;

      /* wait one 0.1msec in the hope that the buffer clears up */
      ts.tv_sec = 0;
      ts.tv_nsec = self->window_full_sleep_nsec;
      nanosleep(&ts, NULL);
    }
  msg_debug("<<<<<< Source side message processing finish",
            evt_tag_str("instance", self->stats_instance ? self->stats_instance : "internal"),
            log_pipe_location_tag(s),
            evt_tag_printf("msg", "%p", msg));

}

static inline void
_create_ack_tracker_if_not_exists(LogSource *self, gboolean pos_tracked)
{
  if (!self->ack_tracker)
    {
      if (pos_tracked)
        self->ack_tracker = late_ack_tracker_new(self);
      else
        self->ack_tracker = early_ack_tracker_new(self);
    }
}

void
log_source_set_options(LogSource *self, LogSourceOptions *options,
                       const gchar *stats_id, const gchar *stats_instance,
                       gboolean threaded, gboolean pos_tracked, LogExprNode *expr_node)
{
  /* NOTE: we don't adjust window_size even in case it was changed in the
   * configuration and we received a SIGHUP.  This means that opened
   * connections will not have their window_size changed. */

  if (g_atomic_counter_get(&self->window_size) == -1)
    g_atomic_counter_set(&self->window_size, options->init_window_size);
  self->options = options;
  if (self->stats_id)
    g_free(self->stats_id);
  self->stats_id = stats_id ? g_strdup(stats_id) : NULL;
  if (self->stats_instance)
    g_free(self->stats_instance);
  self->stats_instance = stats_instance ? g_strdup(stats_instance): NULL;
  self->threaded = threaded;
  self->pos_tracked = pos_tracked;
  self->super.expr_node = expr_node;
  _create_ack_tracker_if_not_exists(self, pos_tracked);
}

void
log_source_init_instance(LogSource *self, GlobalConfig *cfg)
{
  log_pipe_init_instance(&self->super, cfg);
  self->super.queue = log_source_queue;
  self->super.free_fn = log_source_free;
  self->super.init = log_source_init;
  self->super.deinit = log_source_deinit;
  g_atomic_counter_set(&self->window_size, -1);
  self->ack_tracker = NULL;
}

void
log_source_free(LogPipe *s)
{
  LogSource *self = (LogSource *) s;

  g_free(self->stats_id);
  g_free(self->stats_instance);
  log_pipe_free_method(s);

  ack_tracker_free(self->ack_tracker);
}

void
log_source_options_defaults(LogSourceOptions *options)
{
  options->init_window_size = 100;
  options->keep_hostname = -1;
  options->chain_hostnames = -1;
  options->keep_timestamp = -1;
  options->program_override_len = -1;
  options->host_override_len = -1;
  options->tags = NULL;
  options->read_old_records = TRUE;
  host_resolve_options_defaults(&options->host_resolve_options);
}

/* NOTE: _init needs to be idempotent when called multiple times w/o invoking _destroy */
void
log_source_options_init(LogSourceOptions *options, GlobalConfig *cfg, const gchar *group_name)
{
  gchar *source_group_name;

  options->source_queue_callbacks = cfg->source_mangle_callback_list;

  if (options->keep_hostname == -1)
    options->keep_hostname = cfg->keep_hostname;
  if (options->chain_hostnames == -1)
    options->chain_hostnames = cfg->chain_hostnames;
  if (options->keep_timestamp == -1)
    options->keep_timestamp = cfg->keep_timestamp;
  options->group_name = group_name;

  source_group_name = g_strdup_printf(".source.%s", group_name);
  options->source_group_tag = log_tags_get_by_name(source_group_name);
  g_free(source_group_name);
  host_resolve_options_init(&options->host_resolve_options, &cfg->host_resolve_options);
}

void
log_source_options_destroy(LogSourceOptions *options)
{
  host_resolve_options_destroy(&options->host_resolve_options);
  if (options->program_override)
    g_free(options->program_override);
  if (options->host_override)
    g_free(options->host_override);
  if (options->tags)
    {
      g_array_free(options->tags, TRUE);
      options->tags = NULL;
    }
}

void
log_source_options_set_tags(LogSourceOptions *options, GList *tags)
{
  LogTagId id;

  if (!options->tags)
    options->tags = g_array_new(FALSE, FALSE, sizeof(LogTagId));

  while (tags)
    {
      id = log_tags_get_by_name((gchar *) tags->data);
      g_array_append_val(options->tags, id);

      g_free(tags->data);
      tags = g_list_delete_link(tags, tags);
    }
}

void
log_source_global_init(void)
{
  accurate_nanosleep = check_nanosleep();
  if (!accurate_nanosleep)
    {
      msg_debug("nanosleep() is not accurate enough to introduce minor stalls on the reader side, multi-threaded performance may be affected");
    }
}
