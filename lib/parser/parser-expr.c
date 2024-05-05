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

#include "parser/parser-expr.h"
#include "template/templates.h"
#include "logmatcher.h"

#include <string.h>

/* NOTE: consumes template */
void
log_parser_set_template(LogParser *self, LogTemplate *template_obj)
{
  log_template_unref(self->template_obj);
  self->template_obj = template_obj;
}

void
log_parser_clone_settings(LogParser *self, LogParser *cloned)
{
  log_pipe_clone_method(&cloned->super, &self->super);
  log_parser_set_template(cloned, log_template_ref(self->template_obj));
}

gboolean
log_parser_process_message(LogParser *self, LogMessage **pmsg, const LogPathOptions *path_options)
{
  LogMessage *msg = *pmsg;
  gboolean success;

  if (G_LIKELY(!self->template_obj))
    {
      NVTable *payload = nv_table_ref(msg->payload);
      const gchar *value;
      gssize value_len;

      /* NOTE: the process function may set values in the LogMessage
       * instance, which in turn can trigger nv_table_realloc() to be
       * called.  However in case nv_table_realloc() finds a refcounter > 1,
       * it'll always _move_ the structure and leave the old one intact,
       * until its refcounter drops to zero.  If that wouldn't be the case,
       * nv_table_realloc() could make our payload pointer and the
       * LM_V_MESSAGE pointer we pass to process() go stale.
       */

      value = log_msg_get_value(msg, LM_V_MESSAGE, &value_len);
      success = self->process(self, pmsg, path_options, value, value_len);
      nv_table_unref(payload);
    }
  else
    {
      GString *input = g_string_sized_new(256);

      log_template_format(self->template_obj, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, input);
      success = self->process(self, pmsg, path_options, input->str, input->len);
      g_string_free(input, TRUE);
    }

  if (!success)
    stats_counter_inc(self->super.discarded_messages);
  else
    stats_counter_inc(self->processed_messages);

  return success;
}

void
log_parser_queue_method(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogParser *self = (LogParser *) s;
  gboolean success;

  msg_trace(">>>>>> parser rule evaluation begin",
            evt_tag_str("rule", self->name),
            log_pipe_location_tag(s),
            evt_tag_msg_reference(msg));

  success = log_parser_process_message(self, &msg, path_options);

  msg_trace("<<<<<< parser rule evaluation result",
            evt_tag_str("result", success ? "accepted" : "rejected"),
            evt_tag_str("rule", self->name),
            log_pipe_location_tag(s),
            evt_tag_msg_reference(msg));

  if (success)
    {
      log_pipe_forward_msg(s, msg, path_options);
    }
  else
    {
      if (path_options->matched)
        (*path_options->matched) = FALSE;
      log_msg_drop(msg, path_options, AT_PROCESSED);
    }
}

static void
_register_counters(LogParser *self)
{
  gint level = log_pipe_is_internal(&self->super) ? STATS_LEVEL3 : STATS_LEVEL1;

  stats_lock();
  StatsClusterKey sc_key;
  StatsClusterLabel labels[] = { stats_cluster_label("id", self->name) };
  stats_cluster_logpipe_key_set(&sc_key, "parsed_events_total", labels, G_N_ELEMENTS(labels));
  stats_cluster_logpipe_key_add_legacy_alias(&sc_key, SCS_PARSER, self->name, NULL );
  stats_register_counter(level, &sc_key, SC_TYPE_DISCARDED, &self->super.discarded_messages);
  stats_register_counter(level, &sc_key, SC_TYPE_PROCESSED, &self->processed_messages);
  stats_unlock();
}

gboolean
log_parser_init_method(LogPipe *s)
{
  LogParser *self = (LogParser *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!self->name && s->expr_node)
    self->name = cfg_tree_get_rule_name(&cfg->tree, ENC_PARSER, s->expr_node);

  _register_counters(self);

  return TRUE;
}

static void
_unregister_stats(LogParser *self)
{
  stats_lock();
  StatsClusterKey sc_key;
  StatsClusterLabel labels[] = { stats_cluster_label("id", self->name) };
  stats_cluster_logpipe_key_set(&sc_key, "parsed_events_total", labels, G_N_ELEMENTS(labels));
  stats_cluster_logpipe_key_add_legacy_alias(&sc_key, SCS_PARSER, self->name, NULL );
  stats_unregister_counter(&sc_key, SC_TYPE_DISCARDED, &self->super.discarded_messages);
  stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &self->processed_messages);
  stats_unlock();
}

gboolean
log_parser_deinit_method(LogPipe *s)
{
  LogParser *self = (LogParser *) s;

  _unregister_stats(self);

  return TRUE;
}

void
log_parser_free_method(LogPipe *s)
{
  LogParser *self = (LogParser *) s;

  g_free(self->name);
  log_template_unref(self->template_obj);
  log_pipe_free_method(s);
}

void
log_parser_init_instance(LogParser *self, GlobalConfig *cfg)
{
  log_pipe_init_instance(&self->super, cfg);
  self->super.flags |= PIF_CONFIG_RELATED + PIF_SYNC_FILTERX;
  self->super.init = log_parser_init_method;
  self->super.deinit = log_parser_deinit_method;
  self->super.free_fn = log_parser_free_method;
  self->super.queue = log_parser_queue_method;
}
