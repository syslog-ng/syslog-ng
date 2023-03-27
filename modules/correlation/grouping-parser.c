/*
 * Copyright (c) 2015 BalaBit
 * Copyright (c) 2023 Balazs Scheidler <bazsi77@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "grouping-parser.h"

void
grouping_parser_set_key_template(LogParser *s, LogTemplate *key_template)
{
  GroupingParser *self = (GroupingParser *) s;

  log_template_unref(self->key_template);
  self->key_template = log_template_ref(key_template);
}

void
grouping_parser_set_sort_key_template(LogParser *s, LogTemplate *sort_key)
{
  GroupingParser *self = (GroupingParser *) s;

  log_template_unref(self->sort_key_template);
  self->sort_key_template = log_template_ref(sort_key);
}

void
grouping_parser_set_scope(LogParser *s, CorrelationScope scope)
{
  GroupingParser *self = (GroupingParser *) s;

  self->scope = scope;
}

void
grouping_parser_set_timeout(LogParser *s, gint timeout)
{
  GroupingParser *self = (GroupingParser *) s;

  self->timeout = timeout;
}

/*
 * This function can be called any time when pattern-db is not processing
 * messages, but we expect the correlation timer to move forward.  It
 * doesn't need to be called absolutely regularly as it'll use the current
 * system time to determine how much time has passed since the last
 * invocation.  See the timing comment at pattern_db_process() for more
 * information.
 */
static void
_advance_time_by_timer_tick(GroupingParser *self)
{
  StatefulParserEmittedMessages emitted_messages = STATEFUL_PARSER_EMITTED_MESSAGES_INIT;

  if (correlation_state_timer_tick(self->correlation, &emitted_messages))
    {
      msg_debug("Advancing grouping-by() current time because of timer tick",
                evt_tag_long("utc", correlation_state_get_time(self->correlation)),
                log_pipe_location_tag(&self->super.super.super));
    }
  stateful_parser_emitted_messages_flush(&emitted_messages, &self->super);
}

static void
_timer_tick(gpointer s)
{
  GroupingParser *self = (GroupingParser *) s;

  _advance_time_by_timer_tick(self);
  iv_validate_now();
  self->tick.expires = iv_now;
  self->tick.expires.tv_sec++;
  iv_timer_register(&self->tick);
}


static void
_load_correlation_state(GroupingParser *self, GlobalConfig *cfg)
{
  CorrelationState *persisted_correlation = cfg_persist_config_fetch(cfg,
                                            log_pipe_get_persist_name(&self->super.super.super));
  if (persisted_correlation)
    {
      correlation_state_unref(self->correlation);
      self->correlation = persisted_correlation;
    }

  timer_wheel_set_associated_data(self->correlation->timer_wheel, log_pipe_ref((LogPipe *)self),
                                  (GDestroyNotify)log_pipe_unref);
}

static void
_store_data_in_persist(GroupingParser *self, GlobalConfig *cfg)
{
  cfg_persist_config_add(cfg, log_pipe_get_persist_name(&self->super.super.super),
                         correlation_state_ref(self->correlation),
                         (GDestroyNotify) correlation_state_unref);
}

gboolean
grouping_parser_init_method(LogPipe *s)
{
  GroupingParser *self = (GroupingParser *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (self->timeout < 1)
    {
      msg_error("timeout() needs to be specified explicitly and must be greater than 0 in the grouping-by() parser",
                log_pipe_location_tag(s));
      return FALSE;
    }
  if (!self->key_template)
    {
      msg_error("The key() option is mandatory for the grouping-by() parser",
                log_pipe_location_tag(s));
      return FALSE;
    }

  iv_validate_now();
  IV_TIMER_INIT(&self->tick);
  self->tick.cookie = self;
  self->tick.handler = _timer_tick;
  self->tick.expires = iv_now;
  self->tick.expires.tv_sec++;
  self->tick.expires.tv_nsec = 0;
  iv_timer_register(&self->tick);

  _load_correlation_state(self, cfg);

  return stateful_parser_init_method(s);
}

gboolean
grouping_parser_deinit_method(LogPipe *s)
{
  GroupingParser *self = (GroupingParser *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (iv_timer_registered(&self->tick))
    {
      iv_timer_unregister(&self->tick);
    }

  _store_data_in_persist(self, cfg);
  return stateful_parser_deinit_method(s);
}

void
grouping_parser_free_method(LogPipe *s)
{
  GroupingParser *self = (GroupingParser *) s;

  log_template_unref(self->key_template);
  log_template_unref(self->sort_key_template);
  correlation_state_unref(self->correlation);

  stateful_parser_free_method(s);
}

void
grouping_parser_init_instance(GroupingParser *self, GlobalConfig *cfg)
{
  stateful_parser_init_instance(&self->super, cfg);
  self->super.super.super.free_fn = grouping_parser_free_method;
  self->super.super.super.init = grouping_parser_init_method;
  self->super.super.super.deinit = grouping_parser_deinit_method;
//  self->super.super.super.clone = _clone;
//  self->super.super.super.generate_persist_name = _format_persist_name;
//  self->super.super.process = _process;
  self->scope = RCS_GLOBAL;
  self->timeout = -1;
  self->correlation = correlation_state_new();
}
