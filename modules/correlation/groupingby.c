/*
 * Copyright (c) 2015 BalaBit
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

#include "groupingby.h"
#include "correlation.h"
#include "correlation-context.h"
#include "synthetic-message.h"
#include "messages.h"
#include "str-utils.h"
#include "scratch-buffers.h"
#include "filter/filter-expr.h"
#include "timeutils/cache.h"
#include "timeutils/misc.h"
#include <iv.h>

typedef struct _GroupingBy
{
  GroupingParser super;
  SyntheticMessage *synthetic_message;
  FilterExprNode *trigger_condition_expr;
  FilterExprNode *where_condition_expr;
  FilterExprNode *having_condition_expr;
  gchar *prefix;
  gint clone_id;
} GroupingBy;

/* public functions */

void
grouping_by_set_trigger_condition(LogParser *s, FilterExprNode *filter_expr)
{
  GroupingBy *self = (GroupingBy *) s;

  self->trigger_condition_expr = filter_expr;
}

void
grouping_by_set_where_condition(LogParser *s, FilterExprNode *filter_expr)
{
  GroupingBy *self = (GroupingBy *) s;

  self->where_condition_expr = filter_expr;
}

void
grouping_by_set_having_condition(LogParser *s, FilterExprNode *filter_expr)
{
  GroupingBy *self = (GroupingBy *) s;

  self->having_condition_expr = filter_expr;
}

void
grouping_by_set_synthetic_message(LogParser *s, SyntheticMessage *message)
{
  GroupingBy *self = (GroupingBy *) s;

  if (self->synthetic_message)
    synthetic_message_free(self->synthetic_message);
  self->synthetic_message = message;
}

void
grouping_by_set_prefix(LogParser *s, const gchar *prefix)
{
  GroupingBy *self = (GroupingBy *) s;

  g_free(self->prefix);
  self->prefix = g_strdup(prefix);
}


static gboolean
_evaluate_filter(FilterExprNode *expr, CorrelationContext *context)
{
  return filter_expr_eval_with_context(expr,
                                       (LogMessage **) context->messages->pdata,
                                       context->messages->len, &DEFAULT_TEMPLATE_EVAL_OPTIONS);
}

static gboolean
_evaluate_having(GroupingBy *self, CorrelationContext *context)
{
  if (!self->having_condition_expr)
    return TRUE;

  return _evaluate_filter(self->having_condition_expr, context);
}

static gboolean
_evaluate_trigger(GroupingBy *self, CorrelationContext *context)
{
  if (!self->trigger_condition_expr)
    return FALSE;

  return _evaluate_filter(self->trigger_condition_expr, context);
}

static LogMessage *
_aggregate_context(GroupingParser *s, CorrelationContext *context)
{
  GroupingBy *self = (GroupingBy *) s;
  LogMessage *msg = NULL;

  if (!_evaluate_having(self, context))
    {
      msg_debug("groupingby() dropping context, because having() is FALSE",
                evt_tag_str("key", context->key.session_id),
                log_pipe_location_tag(&self->super.super.super.super));
      return NULL;
    }

  msg = synthetic_message_generate_with_context(self->synthetic_message, context);

  return msg;
}

static void
_perform_groupby(GroupingParser *s, LogMessage *msg)
{
  GroupingBy *self = (GroupingBy *) s;

  correlation_state_tx_begin(self->super.correlation);

  CorrelationContext *context = grouping_parser_lookup_or_create_context(&self->super, msg);
  g_ptr_array_add(context->messages, log_msg_ref(msg));

  if (_evaluate_trigger(self, context))
    {
      msg_verbose("Correlation trigger() met, closing state",
                  evt_tag_str("key", context->key.session_id),
                  evt_tag_int("timeout", self->super.timeout),
                  evt_tag_int("num_messages", context->messages->len),
                  log_pipe_location_tag(&self->super.super.super.super));

      /* close down state */
      LogMessage *genmsg = grouping_parser_aggregate_context(&self->super, context);

      correlation_state_tx_end(self->super.correlation);
      if (genmsg)
        {
          stateful_parser_emit_synthetic(&self->super.super, genmsg);
          log_msg_unref(genmsg);
        }

      log_msg_write_protect(msg);
    }
  else
    {
      correlation_state_tx_update_context(self->super.correlation, context, self->super.timeout);
      log_msg_write_protect(msg);

      correlation_state_tx_end(self->super.correlation);
    }
}

static gboolean
_evaluate_where(GroupingParser *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  GroupingBy *self = (GroupingBy *) s;

  if (!self->where_condition_expr)
    return TRUE;

  return filter_expr_eval_root(self->where_condition_expr, pmsg, path_options);
}

static const gchar *
_format_persist_name(const LogPipe *s)
{
  static gchar persist_name[512];
  GroupingBy *self = (GroupingBy *)s;

  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "grouping-by.%s(clone=%d)", s->persist_name, self->clone_id);
  else
    g_snprintf(persist_name, sizeof(persist_name), "grouping-by(%s,scope=%d,clone=%d)", self->super.key_template->template,
               self->super.scope, self->clone_id);

  return persist_name;
}

static gboolean
_init(LogPipe *s)
{
  GroupingBy *self = (GroupingBy *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!self->synthetic_message)
    {
      msg_error("The aggregate() option for grouping-by() is mandatory",
                log_pipe_location_tag(s));
      return FALSE;
    }

  synthetic_message_set_prefix(self->synthetic_message, self->prefix);

  if (self->trigger_condition_expr && !filter_expr_init(self->trigger_condition_expr, cfg))
    return FALSE;
  if (self->where_condition_expr && !filter_expr_init(self->where_condition_expr, cfg))
    return FALSE;
  if (self->having_condition_expr && !filter_expr_init(self->having_condition_expr, cfg))
    return FALSE;

  return grouping_parser_init_method(s);
}

static LogPipe *
_clone(LogPipe *s)
{
  GroupingBy *self = (GroupingBy *) s;
  GroupingBy *cloned;

  cloned = (GroupingBy *) grouping_by_new(s->cfg);
  grouping_parser_set_key_template(&cloned->super.super.super, self->super.key_template);
  grouping_parser_set_sort_key_template(&cloned->super.super.super, self->super.sort_key_template);
  grouping_parser_set_timeout(&cloned->super.super.super, self->super.timeout);
  grouping_parser_set_scope(&cloned->super.super.super, self->super.scope);
  grouping_by_set_synthetic_message(&cloned->super.super.super, self->synthetic_message);
  grouping_by_set_trigger_condition(&cloned->super.super.super, filter_expr_clone(self->trigger_condition_expr));
  grouping_by_set_where_condition(&cloned->super.super.super, filter_expr_clone(self->where_condition_expr));
  grouping_by_set_having_condition(&cloned->super.super.super, filter_expr_clone(self->having_condition_expr));
  grouping_by_set_prefix(&cloned->super.super.super, self->prefix);

  cloned->clone_id = self->clone_id++;
  return &cloned->super.super.super.super;
}

static void
_free(LogPipe *s)
{
  GroupingBy *self = (GroupingBy *) s;

  g_free(self->prefix);
  if (self->synthetic_message)
    synthetic_message_free(self->synthetic_message);

  filter_expr_unref(self->trigger_condition_expr);
  filter_expr_unref(self->where_condition_expr);
  filter_expr_unref(self->having_condition_expr);
  grouping_parser_free_method(s);
}

LogParser *
grouping_by_new(GlobalConfig *cfg)
{
  GroupingBy *self = g_new0(GroupingBy, 1);

  grouping_parser_init_instance(&self->super, cfg);
  self->super.super.super.super.free_fn = _free;
  self->super.super.super.super.init = _init;
  self->super.super.super.super.clone = _clone;
  self->super.super.super.super.generate_persist_name = _format_persist_name;
  self->super.filter_messages = _evaluate_where;
  self->super.perform_grouping = _perform_groupby;
  self->super.aggregate_context = _aggregate_context;
  return &self->super.super.super;
}
