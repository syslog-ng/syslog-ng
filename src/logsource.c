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
  
#include "logsource.h"
#include "messages.h"
#include "misc.h"
#include "timeutils.h"
#include "stats.h"
#include "tags.h"

static void
log_source_msg_ack(LogMessage *msg, gpointer user_data)
{
  LogSource *self = (LogSource *) user_data;
  guint32 old_window_size;
  
  old_window_size = g_atomic_counter_get(&self->options->window_size);
  g_atomic_counter_inc(&self->options->window_size);
  
  /* NOTE: this check could be racy, but it isn't for the following reasons:
   *  - if the current window size went down to zero, the source does not emit new messages
   *  - the only possible change in this case is: the destinations processed a 
   *    message and it got acked back
   *  - so the 0 -> change can only happen because of the destinations
   *  - from all the possible destinations the ACK happens at the last one, thus there 
   *    might be no concurrencies here, even if all destinations are in different threads
   *    
   */
  if (old_window_size == 0)
    main_loop_wakeup();
  log_msg_unref(msg);
  
  log_pipe_unref(&self->super);
}

void
log_source_mangle_hostname(LogSource *self, LogMessage *msg)
{
  gchar *resolved_name = NULL;
  gint resolved_name_len;
  const gchar *orig_host;
  
  resolve_sockaddr(&resolved_name, msg->saddr, self->options->use_dns, self->options->use_fqdn, self->options->use_dns_cache, self->options->normalize_hostnames);
  resolved_name_len = strlen(resolved_name);
  log_msg_set_value(msg, LM_V_HOST_FROM, resolved_name, resolved_name_len);

  orig_host = log_msg_get_value(msg, LM_V_HOST, NULL);
  if (!self->options->keep_hostname || !orig_host)
    {
      gchar host[256];
      gint host_len = -1;
      if (G_UNLIKELY(self->options->chain_hostnames)) 
	{
          msg->flags |= LF_CHAINED_HOSTNAME;
	  if (msg->flags & LF_LOCAL) 
	    {
	      /* local */
	      host_len = g_snprintf(host, sizeof(host), "%s@%s", self->options->group_name, resolved_name);
	    }
	  else if (!orig_host)
	    {
	      /* remote && no hostname */
	      host_len = g_snprintf(host, sizeof(host), "%s/%s", resolved_name, resolved_name);
	    } 
	  else 
	    {
	      /* everything else, append source hostname */
	      if (orig_host)
		host_len = g_snprintf(host, sizeof(host), "%s/%s", orig_host, resolved_name);
	      else
                {
                  strncpy(host, resolved_name, sizeof(host));
                  /* just in case it is not zero terminated */
                  host[255] = 0;
                }
	    }
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
  stats_register_counter(self->stats_level, self->stats_source | SCS_SOURCE, self->stats_id, self->stats_instance, SC_TYPE_PROCESSED, &self->recvd_messages);
  stats_register_counter(self->stats_level, self->stats_source | SCS_SOURCE, self->stats_id, self->stats_instance, SC_TYPE_STAMP, &self->last_message_seen);
  return TRUE;
}

gboolean
log_source_deinit(LogPipe *s)
{
  LogSource *self = (LogSource *) s;
  
  stats_unregister_counter(self->stats_source | SCS_SOURCE, self->stats_id, self->stats_instance, SC_TYPE_PROCESSED, &self->recvd_messages);
  stats_unregister_counter(self->stats_source | SCS_SOURCE, self->stats_id, self->stats_instance, SC_TYPE_STAMP, &self->last_message_seen);
  return TRUE;
}


static void
log_source_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogSource *self = (LogSource *) s;
  LogPathOptions local_options = *path_options;
  guint32 *processed_counter, *stamp;
  gboolean new;
  StatsCounter *handle;
  
  msg_set_context(msg);

  if (msg->timestamps[LM_TS_STAMP].time.tv_sec == -1 || !self->options->keep_timestamp)
    msg->timestamps[LM_TS_STAMP] = msg->timestamps[LM_TS_RECVD];
    
  g_assert(msg->timestamps[LM_TS_STAMP].zone_offset != -1);

  log_source_mangle_hostname(self, msg);

  if (self->options->program_override)
    {
      if (self->options->program_override_len < 0)
        self->options->program_override_len = strlen(self->options->program_override);
      log_msg_set_value(msg, LM_V_PROGRAM, self->options->program_override, self->options->program_override_len);
    }
  if (self->options->host_override)
    {
      if (self->options->host_override_len < 0)
        self->options->host_override_len = strlen(self->options->host_override);
      log_msg_set_value(msg, LM_V_PROGRAM, self->options->host_override, self->options->host_override_len);
    }
    
  handle = stats_register_dynamic_counter(2, SCS_HOST | SCS_SOURCE, NULL, log_msg_get_value(msg, LM_V_HOST, NULL), SC_TYPE_PROCESSED, &processed_counter, &new);
  stats_register_associated_counter(handle, SC_TYPE_STAMP, &stamp);
  stats_counter_inc(processed_counter);
  stats_counter_set(stamp, msg->timestamps[LM_TS_RECVD].time.tv_sec);
  stats_unregister_dynamic_counter(handle, SC_TYPE_PROCESSED, &processed_counter);
  stats_unregister_dynamic_counter(handle, SC_TYPE_STAMP, &stamp);

  /* NOTE: we start by enabling flow-control, thus we need an acknowledgement */
  local_options.flow_control = TRUE;
  log_msg_ref(msg);
  log_msg_add_ack(msg, &local_options);
  msg->ack_func = log_source_msg_ack;
  msg->ack_userdata = log_pipe_ref(s);
    
  g_atomic_counter_dec_and_test(&self->options->window_size);

  /* NOTE: we don't need to be very accurate here, it is a bug if
   * window_size goes below 0, but an atomic operation is expensive and
   * reading it without locks does not always get the most accurate value,
   * but the only outcome might be that window_size is larger (since the
   * update we can lose is the atomic decrement in the consuming thread,
   * most probably SQL), thus the assert is safe.
   */

  g_assert(g_atomic_counter_racy_get(&self->options->window_size) >= 0);

  stats_counter_inc(self->recvd_messages);
  stats_counter_set(self->last_message_seen, msg->timestamps[LM_TS_RECVD].time.tv_sec);
  log_pipe_queue(s->pipe_next, msg, &local_options);
  msg_set_context(NULL);
}

void
log_source_set_options(LogSource *self, LogSourceOptions *options, gint stats_level, gint stats_source, const gchar *stats_id, const gchar *stats_instance)
{
  gint current_window;
  
  if (self->options)
    current_window = g_atomic_counter_get(&self->options->window_size);
  else
    current_window = options->init_window_size;
  self->options = options;
  g_atomic_counter_set(&self->options->window_size, current_window);
  self->stats_level = stats_level;
  self->stats_source = stats_source;
  self->stats_id = stats_id ? g_strdup(stats_id) : NULL;
  self->stats_instance = stats_instance ? g_strdup(stats_instance): NULL;  
}

void
log_source_init_instance(LogSource *self)
{
  log_pipe_init_instance(&self->super);
  self->super.queue = log_source_queue;
  self->super.free_fn = log_source_free;
  self->super.init = log_source_init;
  self->super.deinit = log_source_deinit;
}

void
log_source_free(LogPipe *s)
{
  LogSource *self = (LogSource *) s;
  
  g_free(self->stats_id);
  g_free(self->stats_instance);
  log_pipe_free(s);
}

void
log_source_options_defaults(LogSourceOptions *options)
{
  options->init_window_size = -1;
  g_atomic_counter_set(&options->window_size, -1);
  options->keep_hostname = -1;
  options->chain_hostnames = -1;
  options->use_dns = -1;
  options->use_fqdn = -1;
  options->use_dns_cache = -1;
  options->normalize_hostnames = -1;
  options->keep_timestamp = -1;
}

/*
 * NOTE: options_init and options_destroy are a bit weird, because their
 * invocation is not completely symmetric:
 *
 *   - init is called from driver init (e.g. affile_sd_init), 
 *   - destroy is called from driver free method (e.g. affile_sd_free, NOT affile_sd_deinit)
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
log_source_options_init(LogSourceOptions *options, GlobalConfig *cfg, const gchar *group_name)
{
  gchar *host_override, *program_override;
  gchar *source_group_name;
  
  host_override = options->host_override;
  options->host_override = NULL;
  program_override = options->program_override;
  options->program_override = NULL;
  log_source_options_destroy(options);
  
  options->host_override = host_override;
  options->host_override_len = -1;
  options->program_override = program_override;
  options->program_override_len = -1;
  
  if (options->init_window_size == -1)
    options->init_window_size = cfg->log_iw_size;
  if (g_atomic_counter_get(&options->window_size) == -1)
    g_atomic_counter_set(&options->window_size, options->init_window_size);
  if (options->keep_hostname == -1)
    options->keep_hostname = cfg->keep_hostname;
  if (options->chain_hostnames == -1)
    options->chain_hostnames = cfg->chain_hostnames;
  if (options->use_dns == -1)
    options->use_dns = cfg->use_dns;
  if (options->use_fqdn == -1)
    options->use_fqdn = cfg->use_fqdn;
  if (options->use_dns_cache == -1)
    options->use_dns_cache = cfg->use_dns_cache;
  if (options->normalize_hostnames == -1)
    options->normalize_hostnames = cfg->normalize_hostnames;
  if (options->keep_timestamp == -1)
    options->keep_timestamp = cfg->keep_timestamp;
  options->group_name = group_name;

  source_group_name = g_strdup_printf(".source.%s", group_name);
  options->source_group_tag = log_tags_get_by_name(source_group_name);
  g_free(source_group_name);
}

void
log_source_options_destroy(LogSourceOptions *options)
{
  if (options->program_override)
    g_free(options->program_override);
  if (options->host_override)
    g_free(options->host_override);
}
