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
#include "stats/stats-registry.h"
#include "stats/stats-cluster-single.h"
#include "msg-stats.h"
#include "logmsg/tags.h"
#include "ack-tracker/ack_tracker.h"
#include "ack-tracker/ack_tracker_factory.h"
#include "timeutils/misc.h"
#include "scratch-buffers.h"

#include <string.h>
#include <unistd.h>

gboolean accurate_nanosleep = FALSE;

void
log_source_wakeup(LogSource *self)
{
  if (self->wakeup)
    self->wakeup(self);

  msg_diagnostics("Source has been resumed", log_pipe_location_tag(&self->super));
}

static inline guint32
_take_reclaimed_window(LogSource *self, guint32 window_size_increment)
{
  gssize old = atomic_gssize_sub(&self->window_size_to_be_reclaimed, window_size_increment);
  gboolean reclaim_in_progress = (old > 0);

  if (!reclaim_in_progress)
    {
      return window_size_increment;
    }

  guint32 remaining_window_size_increment = MAX(window_size_increment - old, 0);
  guint32 reclaimed = window_size_increment - remaining_window_size_increment;
  atomic_gssize_add(&self->pending_reclaimed, reclaimed);

  return remaining_window_size_increment;
}

static inline void
_flow_control_window_size_adjust(LogSource *self, guint32 window_size_increment, gboolean last_ack_type_is_suspended)
{
  gboolean suspended;

  if (G_UNLIKELY(dynamic_window_is_enabled(&self->dynamic_window)))
    window_size_increment = _take_reclaimed_window(self, window_size_increment);

  gsize old_window_size = window_size_counter_add(&self->window_size, window_size_increment, &suspended);
  stats_counter_add(self->stat_window_size, window_size_increment);

  msg_diagnostics("Window size adjustment",
                  evt_tag_int("old_window_size", old_window_size),
                  evt_tag_int("window_size_increment", window_size_increment),
                  evt_tag_str("suspended_before_increment", suspended ? "TRUE" : "FALSE"),
                  evt_tag_str("last_ack_type_is_suspended", last_ack_type_is_suspended ? "TRUE" : "FALSE"));

  gboolean need_to_resume_counter = !last_ack_type_is_suspended && suspended;
  if (need_to_resume_counter)
    window_size_counter_resume(&self->window_size);
  if (old_window_size == 0 || need_to_resume_counter)
    log_source_wakeup(self);
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
  _flow_control_window_size_adjust(self, window_size_increment, FALSE);
  _flow_control_rate_adjust(self);
}

void
log_source_flow_control_adjust_when_suspended(LogSource *self, guint32 window_size_increment)
{
  _flow_control_window_size_adjust(self, window_size_increment, TRUE);
  _flow_control_rate_adjust(self);
}

void
log_source_disable_bookmark_saving(LogSource *self)
{
  ack_tracker_disable_bookmark_saving(self->ack_tracker);
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
  msg_debug("Source has been suspended",
            log_pipe_location_tag(&self->super),
            evt_tag_str("function", __FUNCTION__));

  window_size_counter_suspend(&self->window_size);
}

void
log_source_enable_dynamic_window(LogSource *self, DynamicWindowPool *window_pool)
{
  dynamic_window_set_pool(&self->dynamic_window, dynamic_window_pool_ref(window_pool));
}

gboolean
log_source_is_dynamic_window_enabled(LogSource *self)
{
  return dynamic_window_is_enabled(&self->dynamic_window);
}

void
log_source_dynamic_window_update_statistics(LogSource *self)
{
  dynamic_window_stat_update(&self->dynamic_window.stat, window_size_counter_get(&self->window_size, NULL));
  msg_trace("Updating dynamic window statistic", evt_tag_int("avg window size",
                                                             dynamic_window_stat_get_avg(&self->dynamic_window.stat)));
}

static void
_reclaim_dynamic_window(LogSource *self, gsize window_size)
{
  g_assert(self->full_window_size - window_size >= self->initial_window_size);
  atomic_gssize_set(&self->window_size_to_be_reclaimed, window_size);
}

static void
_release_dynamic_window(LogSource *self)
{
  g_assert(self->ack_tracker == NULL);

  gsize dynamic_part = self->full_window_size - self->initial_window_size;
  msg_trace("Releasing dynamic part of the window", evt_tag_int("dynamic_window_to_be_released", dynamic_part),
            log_pipe_location_tag(&self->super));

  self->full_window_size -= dynamic_part;
  stats_counter_sub(self->stat_full_window, dynamic_part);

  window_size_counter_sub(&self->window_size, dynamic_part, NULL);
  stats_counter_sub(self->stat_window_size, dynamic_part);
  dynamic_window_release(&self->dynamic_window, dynamic_part);

  dynamic_window_pool_unref(self->dynamic_window.pool);
}

static void
_inc_balanced(LogSource *self, gsize inc)
{
  gsize offered_dynamic = dynamic_window_request(&self->dynamic_window, inc);

  msg_trace("Balance::increase",
            log_pipe_location_tag(&self->super),
            evt_tag_printf("connection", "%p", self),
            evt_tag_int("old_full_window_size", self->full_window_size),
            evt_tag_int("new_full_window_size", self->full_window_size + offered_dynamic));

  self->full_window_size += offered_dynamic;
  stats_counter_add(self->stat_full_window, offered_dynamic);

  gsize old_window_size = window_size_counter_add(&self->window_size, offered_dynamic, NULL);
  stats_counter_add(self->stat_window_size, offered_dynamic);
  if (old_window_size == 0 && offered_dynamic != 0)
    log_source_wakeup(self);
}

static void
_dec_balanced(LogSource *self, gsize dec)
{
  gsize new_full_window_size = self->full_window_size - dec;

  gsize empty_window = window_size_counter_get(&self->window_size, NULL);
  gsize remaining_sub = 0;

  if (empty_window <= dec)
    {
      remaining_sub = dec - empty_window;
      if (empty_window == 0)
        {
          dec = 0;
        }
      else
        {
          dec = empty_window - 1;
        }

      new_full_window_size = self->full_window_size - dec;
      _reclaim_dynamic_window(self, remaining_sub);
    }

  window_size_counter_sub(&self->window_size, dec, NULL);
  stats_counter_sub(self->stat_window_size, dec);

  msg_trace("Balance::decrease",
            log_pipe_location_tag(&self->super),
            evt_tag_printf("connection", "%p", self),
            evt_tag_int("old_full_window_size", self->full_window_size),
            evt_tag_int("new_full_window_size", new_full_window_size),
            evt_tag_int("to_be_reclaimed", remaining_sub));

  self->full_window_size = new_full_window_size;
  stats_counter_set(self->stat_full_window, new_full_window_size);
  dynamic_window_release(&self->dynamic_window, dec);
}

static gboolean
_reclaim_window_instead_of_rebalance(LogSource *self)
{
  //check pending_reclaimed
  gssize total_reclaim = atomic_gssize_set_and_get(&self->pending_reclaimed, 0);
  gssize to_be_reclaimed = atomic_gssize_get(&self->window_size_to_be_reclaimed);
  gboolean reclaim_in_progress = (to_be_reclaimed > 0);

  if (total_reclaim > 0)
    {
      self->full_window_size -= total_reclaim;
      stats_counter_sub(self->stat_full_window, total_reclaim);
      dynamic_window_release(&self->dynamic_window, total_reclaim);
    }
  else
    {
      //to avoid underflow, we need to set a value <= 0
      if (to_be_reclaimed < 0)
        atomic_gssize_set(&self->window_size_to_be_reclaimed, 0);
    }

  msg_trace("Checking if reclaim is in progress...",
            log_pipe_location_tag(&self->super),
            evt_tag_printf("connection", "%p", self),
            evt_tag_printf("in progress", "%s", reclaim_in_progress ? "yes" : "no"),
            evt_tag_long("total_reclaim", total_reclaim));

  return reclaim_in_progress;
}

static void
_dynamic_window_rebalance(LogSource *self)
{
  gsize current_dynamic_win = self->full_window_size - self->initial_window_size;
  gboolean have_to_increase = current_dynamic_win < self->dynamic_window.pool->balanced_window;
  gboolean have_to_decrease = current_dynamic_win > self->dynamic_window.pool->balanced_window;

  msg_trace("Rebalance dynamic window",
            log_pipe_location_tag(&self->super),
            evt_tag_printf("connection", "%p", self),
            evt_tag_int("full_window", self->full_window_size),
            evt_tag_int("dynamic_win", current_dynamic_win),
            evt_tag_int("static_window", self->initial_window_size),
            evt_tag_int("balanced_window", self->dynamic_window.pool->balanced_window),
            evt_tag_int("avg_free", dynamic_window_stat_get_avg(&self->dynamic_window.stat)));

  if (have_to_increase)
    _inc_balanced(self, self->dynamic_window.pool->balanced_window - current_dynamic_win);
  else if (have_to_decrease)
    _dec_balanced(self, current_dynamic_win - self->dynamic_window.pool->balanced_window);
}

void
log_source_dynamic_window_realloc(LogSource *self)
{
  /* it is safe to assume that the window size is not decremented while this function runs,
   * only incrementation is possible by destination threads */

  if (!_reclaim_window_instead_of_rebalance(self))
    _dynamic_window_rebalance(self);

  dynamic_window_stat_reset(&self->dynamic_window.stat);
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
              host_len = g_snprintf(host, sizeof(host), "%s/%s", orig_host, resolved_name);
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

static void
_register_window_stats(LogSource *self)
{
  if (!stats_check_level(4))
    return;

  const gchar *instance_name = self->name ? : self->stats_instance;

  StatsClusterKey sc_key;
  stats_cluster_single_key_set_with_name(&sc_key, self->options->stats_source | SCS_SOURCE, self->stats_id,
                                         instance_name, "free_window");
  self->stat_window_size_cluster = stats_register_dynamic_counter(4, &sc_key, SC_TYPE_SINGLE_VALUE,
                                   &self->stat_window_size);
  stats_counter_set(self->stat_window_size, window_size_counter_get(&self->window_size, NULL));


  stats_cluster_single_key_set_with_name(&sc_key, self->options->stats_source | SCS_SOURCE, self->stats_id,
                                         instance_name, "full_window");
  self->stat_full_window_cluster = stats_register_dynamic_counter(4, &sc_key, SC_TYPE_SINGLE_VALUE,
                                   &self->stat_full_window);
  stats_counter_set(self->stat_full_window, self->full_window_size);

}

static void
_unregister_window_stats(LogSource *self)
{
  if (!stats_check_level(4))
    return;

  stats_unregister_dynamic_counter(self->stat_window_size_cluster, SC_TYPE_SINGLE_VALUE, &self->stat_window_size);
  stats_unregister_dynamic_counter(self->stat_full_window_cluster, SC_TYPE_SINGLE_VALUE, &self->stat_full_window);
}

static inline void
_create_ack_tracker_if_not_exists(LogSource *self)
{
  if (!self->ack_tracker)
    {
      if (!self->ack_tracker_factory)
        self->ack_tracker_factory = instant_ack_tracker_bookmarkless_factory_new();
      self->ack_tracker = ack_tracker_factory_create(self->ack_tracker_factory, self);
    }
}

gboolean
log_source_init(LogPipe *s)
{
  LogSource *self = (LogSource *) s;

  _create_ack_tracker_if_not_exists(self);
  if (!ack_tracker_init(self->ack_tracker))
    {
      msg_error("Failed to initialize AckTracker");
      return FALSE;
    }

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, self->options->stats_source | SCS_SOURCE, self->stats_id, self->stats_instance);
  stats_register_counter(self->options->stats_level, &sc_key,
                         SC_TYPE_PROCESSED, &self->recvd_messages);
  stats_register_counter(self->options->stats_level, &sc_key, SC_TYPE_STAMP, &self->last_message_seen);

  _register_window_stats(self);

  stats_unlock();

  return TRUE;
}

gboolean
log_source_deinit(LogPipe *s)
{
  LogSource *self = (LogSource *) s;
  ack_tracker_deinit(self->ack_tracker);

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, self->options->stats_source | SCS_SOURCE, self->stats_id, self->stats_instance);
  stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &self->recvd_messages);
  stats_unregister_counter(&sc_key, SC_TYPE_STAMP, &self->last_message_seen);

  _unregister_window_stats(self);

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

  old_window_size = window_size_counter_sub(&self->window_size, 1, NULL);
  stats_counter_sub(self->stat_window_size, 1);

  if (G_UNLIKELY(old_window_size == 1))
    {
      msg_debug("Source has been suspended",
                log_pipe_location_tag(&self->super),
                evt_tag_str("function", __FUNCTION__));
    }

  /*
   * NOTE: this assertion validates that the source is not overflowing its
   * own flow-control window size, decreased above, by the atomic statement.
   *
   * If the _old_ value is zero, that means that the decrement operation
   * above has decreased the value to -1.
   */

  g_assert(old_window_size > 0);

  ScratchBuffersMarker mark;
  scratch_buffers_mark(&mark);
  log_pipe_queue(&self->super, msg, &path_options);
  scratch_buffers_reclaim_marked(mark);
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
          if(!((mangle_callback) (next_item->data))(log_pipe_get_config(s), msg, self))
            {
              log_msg_drop(msg, path_options, AT_PROCESSED);
              return FALSE;
            }
        }
      next_item = next_item->next;
    }
  return TRUE;
}

static void
log_source_override_host(LogSource *self, LogMessage *msg)
{
  if (self->options->host_override_len < 0)
    self->options->host_override_len = strlen(self->options->host_override);
  log_msg_set_value(msg, LM_V_HOST, self->options->host_override, self->options->host_override_len);
}

static void
log_source_override_program(LogSource *self, LogMessage *msg)
{
  if (self->options->program_override_len < 0)
    self->options->program_override_len = strlen(self->options->program_override);
  log_msg_set_value(msg, LM_V_PROGRAM, self->options->program_override, self->options->program_override_len);
}

static gchar *
_get_pid_string(void)
{
#define MAX_PID_CHAR_COUNT 20 /* max PID on 64 bit systems is 2^64 - 1, which is 19 characters, +1 for terminating 0 */

  static gchar pid_string[MAX_PID_CHAR_COUNT];

  if (pid_string[0] == '\0')
    {
#ifdef _WIN32
      g_snprintf(pid_string, MAX_PID_CHAR_COUNT, "%lu", GetCurrentProcessId());
#else
      g_snprintf(pid_string, MAX_PID_CHAR_COUNT, "%d", getpid());
#endif
    }

  return pid_string;
}

static void
log_source_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogSource *self = (LogSource *) s;
  gint i;

  msg_set_context(msg);

  msg_diagnostics(">>>>>> Source side message processing begin",
                  evt_tag_str("instance", self->stats_instance ? self->stats_instance : "internal"),
                  log_pipe_location_tag(s),
                  evt_tag_msg_reference(msg));

  /* $HOST setup */
  log_source_mangle_hostname(self, msg);

  if (self->options->use_syslogng_pid)
    log_msg_set_value(msg, LM_V_PID, _get_pid_string(), -1);

  /* source specific tags */
  if (self->options->tags)
    {
      for (i = 0; i < self->options->tags->len; i++)
        {
          log_msg_set_tag_by_id(msg, g_array_index(self->options->tags, LogTagId, i));
        }
    }

  log_msg_set_tag_by_id(msg, self->options->source_group_tag);


  if (!_invoke_mangle_callbacks(s, msg, path_options))
    return;

  if (self->options->host_override)
    log_source_override_host(self, msg);

  if (self->options->program_override)
    log_source_override_program(self, msg);

  msg_stats_update_counters(self->stats_id, msg);

  /* message setup finished, send it out */

  stats_counter_inc(self->recvd_messages);
  stats_counter_set(self->last_message_seen, msg->timestamps[LM_TS_RECVD].ut_sec);
  log_pipe_forward_msg(s, msg, path_options);

  if (accurate_nanosleep && self->threaded && self->window_full_sleep_nsec > 0 && !log_source_free_to_send(self))
    {
      struct timespec ts;

      /* wait one 0.1msec in the hope that the buffer clears up */
      ts.tv_sec = 0;
      ts.tv_nsec = self->window_full_sleep_nsec;
      nanosleep(&ts, NULL);
    }
  msg_diagnostics("<<<<<< Source side message processing finish",
                  evt_tag_str("instance", self->stats_instance ? self->stats_instance : "internal"),
                  log_pipe_location_tag(s),
                  evt_tag_msg_reference(msg));

  msg_set_context(NULL);
}

static void
_initialize_window(LogSource *self, gint init_window_size)
{
  self->window_initialized = TRUE;
  window_size_counter_set(&self->window_size, init_window_size);

  self->initial_window_size = init_window_size;
  self->full_window_size = init_window_size;
}

static gboolean
_is_window_initialized(LogSource *self)
{
  return self->window_initialized;
}

void
log_source_set_options(LogSource *self, LogSourceOptions *options,
                       const gchar *stats_id, const gchar *stats_instance,
                       gboolean threaded, LogExprNode *expr_node)
{
  /* NOTE: we don't adjust window_size even in case it was changed in the
   * configuration and we received a SIGHUP.  This means that opened
   * connections will not have their window_size changed. */

  if (!_is_window_initialized(self))
    _initialize_window(self, options->init_window_size);

  self->options = options;
  if (self->stats_id)
    g_free(self->stats_id);
  self->stats_id = stats_id ? g_strdup(stats_id) : NULL;
  if (self->stats_instance)
    g_free(self->stats_instance);
  self->stats_instance = stats_instance ? g_strdup(stats_instance): NULL;
  self->threaded = threaded;

  log_pipe_detach_expr_node(&self->super);
  log_pipe_attach_expr_node(&self->super, expr_node);
}

void
log_source_set_ack_tracker_factory(LogSource *self, AckTrackerFactory *factory)
{
  ack_tracker_factory_unref(self->ack_tracker_factory);
  self->ack_tracker_factory = factory;
}

void
log_source_set_name(LogSource *self, const gchar *name)
{
  g_free(self->name);
  self->name = name ? g_strdup(name) : NULL;
}

void
log_source_init_instance(LogSource *self, GlobalConfig *cfg)
{
  log_pipe_init_instance(&self->super, cfg);
  self->super.queue = log_source_queue;
  self->super.free_fn = log_source_free;
  self->super.init = log_source_init;
  self->super.deinit = log_source_deinit;
  self->window_initialized = FALSE;
  self->ack_tracker_factory = instant_ack_tracker_bookmarkless_factory_new();
  self->ack_tracker = NULL;
}

void
log_source_free(LogPipe *s)
{
  LogSource *self = (LogSource *) s;

  ack_tracker_free(self->ack_tracker);
  self->ack_tracker = NULL;

  g_free(self->name);
  g_free(self->stats_id);
  g_free(self->stats_instance);
  log_pipe_detach_expr_node(&self->super);
  log_pipe_free_method(s);

  ack_tracker_factory_unref(self->ack_tracker_factory);
  if (G_UNLIKELY(dynamic_window_is_enabled(&self->dynamic_window)))
    _release_dynamic_window(self);
}

void
log_source_options_defaults(LogSourceOptions *options)
{
  options->init_window_size = -1;
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

  if (options->init_window_size == -1)
    options->init_window_size = 100;
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
