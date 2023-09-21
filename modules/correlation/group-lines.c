/*
 * Copyright (c) 2023 One Identity LLC.
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

#include "group-lines.h"
#include "correlation.h"
#include "correlation-context.h"
#include "scratch-buffers.h"
#include "str-utils.h"
#include "messages.h"
#include "grouping-parser.h"
#include "id-counter.h"

#include <iv.h>

typedef struct _GroupLinesContext
{
  CorrelationContext super;
  MultiLineLogic *multi_line;
  GString *line_buffer;
} GroupLinesContext;

static void
group_lines_context_update(GroupLinesContext *self, LogMessage *msg)
{
  if (self->super.messages->len > 0)
    {
      LogMessage *old_msg = g_ptr_array_index(self->super.messages, 0);

      log_msg_unref(old_msg);
      g_ptr_array_index(self->super.messages, 0) = log_msg_ref(msg);
    }
  else
    g_ptr_array_add(self->super.messages, log_msg_ref(msg));
}

static void
group_lines_context_clear(CorrelationContext *s)
{
  GroupLinesContext *self = (GroupLinesContext *) s;

  g_string_truncate(self->line_buffer, 0);
  correlation_context_clear_method(s);
}

static void
group_lines_context_free(CorrelationContext *s)
{
  GroupLinesContext *self = (GroupLinesContext *) s;

  multi_line_logic_free(self->multi_line);
  g_string_free(self->line_buffer, TRUE);
  correlation_context_free_method(&self->super);
}

GroupLinesContext *
group_lines_context_new(const CorrelationKey *key, MultiLineLogic *multi_line)
{
  GroupLinesContext *self = g_new0(GroupLinesContext, 1);
  correlation_context_init(&self->super, key);
  self->super.free_fn = group_lines_context_free;
  self->super.clear = group_lines_context_clear;
  self->line_buffer = g_string_sized_new(1024);
  self->multi_line = multi_line;
  return self;
}

typedef struct _GroupLines
{
  GroupingParser super;
  guint clone_id;
  IdCounter *id_counter;
  gchar *separator;
  gsize separator_len;
  MultiLineOptions multi_line_options;
} GroupLines;

/* public functions */

MultiLineOptions *
group_lines_get_multi_line_options(LogParser *s)
{
  GroupLines *self = (GroupLines *) s;

  return &self->multi_line_options;
}

void
group_lines_set_separator(LogParser *s, const gchar *separator)
{
  GroupLines *self = (GroupLines *) s;

  g_free(self->separator);
  self->separator = g_strdup(separator);
  self->separator_len = strlen(self->separator);
}

static CorrelationContext *
_construct_context(GroupingParser *s, CorrelationKey *key)
{
  GroupLines *self = (GroupLines *) s;

  return &group_lines_context_new(key, multi_line_factory_construct(&self->multi_line_options))->super;
}

static void
_update_context_add_message(GroupLines *self, GroupLinesContext *context, LogMessage *msg, const gchar *line,
                            gsize line_len)
{
  group_lines_context_update(context, msg);

  msg_debug("group-lines: accumulating new segment into the line",
            evt_tag_str("key", context->super.key.session_id),
            evt_tag_mem("segment", line, line_len),
            evt_tag_str("line", context->line_buffer->str));
  if (context->line_buffer->len)
    g_string_append_len(context->line_buffer, self->separator, self->separator_len);
  g_string_append_len(context->line_buffer, line, line_len);
}

static const gchar *
_get_payload(GroupLines *self, LogMessage *msg, gssize *len)
{
  LogTemplate *template = self->super.super.super.template_obj;

  if (!template)
    return log_msg_get_value(msg, LM_V_MESSAGE, len);

  if (log_template_is_trivial(template))
    return log_template_get_trivial_value(template, msg, len);

  GString *buf = scratch_buffers_alloc();
  log_template_format(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, buf);
  *len = buf->len;
  return buf->str;
}

static GroupingParserUpdateContextResult
_update_context(GroupingParser *s, CorrelationContext *c, LogMessage *msg)
{
  GroupLines *self = (GroupLines *) s;
  GroupLinesContext *context = (GroupLinesContext *) c;

  gssize line_len;
  const gchar *line = _get_payload(self, msg, &line_len);

  gint verdict = multi_line_logic_accumulate_line(context->multi_line, (const guchar *) context->line_buffer->str,
                                                  context->line_buffer->len, (const guchar *) line, line_len);

  if (verdict & MLL_EXTRACTED)
    {
      if (verdict & MLL_CONSUME_SEGMENT)
        {
          gint drop_length = (verdict & MLL_CONSUME_PARTIAL_AMOUNT_MASK) >> MLL_CONSUME_PARTIAL_AMOUNT_SHIFT;
          _update_context_add_message(self, context, msg, line, line_len - drop_length);
          msg_debug("group-lines: accumulated line extracted",
                    evt_tag_str("key", context->super.key.session_id),
                    evt_tag_str("line", context->line_buffer->str));
          return GP_CONTEXT_COMPLETE;
        }
      else if (verdict & MLL_REWIND_SEGMENT)
        {
          msg_debug("group-lines: accumulated line extracted",
                    evt_tag_str("key", context->super.key.session_id),
                    evt_tag_str("line", context->line_buffer->str));
          return GP_STARTS_NEW_CONTEXT;
        }
      else
        g_assert_not_reached();
    }
  else if (verdict & MLL_WAITING)
    {
      if (verdict & MLL_CONSUME_SEGMENT)
        {
          _update_context_add_message(self, context, msg, line, line_len);
          return GP_CONTEXT_UPDATED;
        }
      else
        g_assert_not_reached();
      return FALSE;
    }
  else
    g_assert_not_reached();

}

static LogMessage *
_generate_synthetic_msg(GroupLines *self, GroupLinesContext *context)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  path_options.ack_needed = FALSE;

  LogMessage *msg = log_msg_ref(correlation_context_get_last_message(&context->super));
  log_msg_make_writable(&msg, &path_options);
  return msg;
}

static LogMessage *
_aggregate_context(GroupingParser *s, CorrelationContext *c)
{
  GroupLines *self = (GroupLines *) s;
  GroupLinesContext *context = (GroupLinesContext *) c;

  LogMessage *msg = _generate_synthetic_msg(self, context);

  log_msg_set_value(msg, LM_V_MESSAGE, context->line_buffer->str, context->line_buffer->len);

  return msg;
}

static const gchar *
_format_persist_name(const LogPipe *s)
{
  static gchar persist_name[512];
  GroupLines *self = (GroupLines *)s;

  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "group-lines.%s(clone=%d)", s->persist_name, self->clone_id);
  else
    g_snprintf(persist_name, sizeof(persist_name), "group-lines(%s,scope=%d,clone=%d)",
               self->super.key_template->template_str,
               self->super.scope, self->clone_id);

  return persist_name;
}

static gboolean
_init(LogPipe *s)
{
  GroupLines *self = (GroupLines *) s;

  self->id_counter = NULL;

  if (self->super.timeout < 1)
    {
      msg_error("timeout() needs to be specified explicitly and must be greater than 0 in the group-lines() parser",
                log_pipe_location_tag(s));
      return FALSE;
    }
  if (!self->super.key_template)
    {
      msg_error("The key() option is mandatory for the group-lines() parser",
                log_pipe_location_tag(s));
      return FALSE;
    }

  if (!multi_line_options_validate(&self->multi_line_options))
    return FALSE;
  return grouping_parser_init_method(s);
}

static LogPipe *
_clone(LogPipe *s)
{
  GroupLines *self = (GroupLines *) s;
  GroupLines *cloned;

  cloned = (GroupLines *) group_lines_new(s->cfg);
  grouping_parser_clone_settings(&self->super, &cloned->super);
  group_lines_set_separator(&cloned->super.super.super, self->separator);

  multi_line_options_copy(&cloned->multi_line_options, &self->multi_line_options);

  id_counter_unref(cloned->id_counter);
  cloned->id_counter = id_counter_ref(self->id_counter);
  cloned->clone_id = id_counter_get_next_id(cloned->id_counter);

  return &cloned->super.super.super.super;
}

static void
_free(LogPipe *s)
{
  GroupLines *self = (GroupLines *) s;

  id_counter_unref(self->id_counter);

  multi_line_options_destroy(&self->multi_line_options);
  g_free(self->separator);
  grouping_parser_free_method(s);
}

LogParser *
group_lines_new(GlobalConfig *cfg)
{
  GroupLines *self = g_new0(GroupLines, 1);
  self->id_counter = id_counter_new();
  self->clone_id = id_counter_get_next_id(self->id_counter);

  grouping_parser_init_instance(&self->super, cfg);
  self->super.super.super.super.init = _init;
  self->super.super.super.super.free_fn = _free;
  self->super.super.super.super.clone = _clone;
  self->super.super.super.super.generate_persist_name = _format_persist_name;
  self->super.scope = RCS_GLOBAL;
  self->super.construct_context = _construct_context;
  self->super.update_context = _update_context;
  self->super.aggregate_context = _aggregate_context;
  self->super.super.inject_mode = LDBP_IM_AGGREGATE_ONLY;
  group_lines_set_separator(&self->super.super.super, "\n");
  multi_line_options_defaults(&self->multi_line_options);
  return &self->super.super.super;
}
