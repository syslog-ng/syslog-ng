/*
 * Copyright (c) 2020 Balabit
 * Copyright (c) 2020 Balazs Scheidler <bazsi77@gmail.com>
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

#include "eval.h"
#include "repr.h"
#include "macros.h"
#include "escaping.h"
#include "cfg.h"
#include "scratch-buffers.h"
#include "templates.h"
#include "globals.h"

static LogMessageValueType
_propagate_type(LogMessageValueType acc_type, LogMessageValueType elem_type)
{

  /* NOTE: if this assertion fails, it means that a macro/template function
   * does not return a valid type.  We are not defaulting to strings in this
   * case so we make a conscious decision on typing every time we add a new
   * macro/template function.  */

  g_assert(elem_type != LM_VT_NONE);
  if (acc_type == LM_VT_NONE)
    acc_type = elem_type;
  return acc_type;
}

static gboolean
_should_render(const gchar *value, LogMessageValueType value_type, LogMessageValueType type_hint)
{
  if (value_type == LM_VT_BYTES || value_type == LM_VT_PROTOBUF)
    return value_type == type_hint;

  return !!value[0];
}

static void
log_template_append_elem_value(LogTemplate *self, LogTemplateElem *e, LogTemplateEvalOptions *options,
                               LogMessage *msg, LogMessageValueType *type, GString *result)
{
  const gchar *value = NULL;
  gssize value_len = -1;
  LogMessageValueType value_type = LM_VT_NONE;

  value = log_msg_get_value_with_type(msg, e->value_handle, &value_len, &value_type);
  if (value && _should_render(value, value_type, self->type_hint))
    {
      g_string_append_len(result, value, value_len);
    }
  else if (e->default_value)
    {
      g_string_append_len(result, e->default_value, -1);
      value_type = LM_VT_STRING;
    }
  else if (value_type == LM_VT_BYTES || value_type == LM_VT_PROTOBUF)
    {
      value_type = LM_VT_NULL;
    }
  *type = _propagate_type(*type, value_type);
}

static void
log_template_append_elem_macro(LogTemplate *self, LogTemplateElem *e, LogTemplateEvalOptions *options,
                               LogMessage *msg, LogMessageValueType *type, GString *result)
{
  gint len = result->len;
  LogMessageValueType value_type = LM_VT_NONE;

  if (e->macro)
    {
      log_macro_expand(e->macro, options, msg, result, &value_type);
      if (len == result->len && e->default_value)
        g_string_append(result, e->default_value);
      *type = _propagate_type(*type, value_type);
    }
}

static void
log_template_append_elem_func(LogTemplate *self, LogTemplateElem *e, LogTemplateEvalOptions *options,
                              LogMessage **messages, gint num_messages, gint msg_ndx,
                              LogMessageValueType *type, GString *result)
{
  LogTemplateInvokeArgs args =
  {
    e->msg_ref ? &messages[msg_ndx] : messages,
    e->msg_ref ? 1 : num_messages,
    options,
  };
  LogMessageValueType value_type = LM_VT_NONE;


  /* if a function call is called with an msg_ref, we only
   * pass that given logmsg to argument resolution, otherwise
   * we pass the whole set so the arguments can individually
   * specify which message they want to resolve from
   */
  if (e->func.ops->eval)
    e->func.ops->eval(e->func.ops, e->func.state, &args);
  e->func.ops->call(e->func.ops, e->func.state, &args, result, &value_type);

  *type = _propagate_type(*type, value_type);
}

void
log_template_append_format_value_and_type_with_context(LogTemplate *self, LogMessage **messages, gint num_messages,
                                                       LogTemplateEvalOptions *options,
                                                       GString *result, LogMessageValueType *type)
{
  LogTemplateElem *e;
  LogMessageValueType t = LM_VT_NONE;
  gboolean first_elem = TRUE;
  GString *target_buffer = result;

  if (!options->opts)
    {
      /* try the configuration first */

      if (self->cfg)
        options->opts = &self->cfg->template_options;
      else
        options->opts = log_template_get_global_template_options();
    }

  gboolean escape = (self->escape || (self->top_level && options->opts->escape));
  if (escape)
    target_buffer = scratch_buffers_alloc();

  for (GList *p = self->compiled_template; p; p = g_list_next(p), first_elem = FALSE)
    {
      gint msg_ndx;

      if (!first_elem)
        {
          /* this is the 2nd elem in the compiled template, we are
           * concatenating multiple elements, convert the value to string */

          t = LM_VT_STRING;
        }

      e = (LogTemplateElem *) p->data;
      if (e->text)
        {
          g_string_append_len(result, e->text, e->text_len);
          /* concatenating literal text */
          if (e->text_len)
            t = LM_VT_STRING;
        }

      /* NOTE: msg_ref is 1 larger than the index specified by the user in
       * order to make it distinguishable from the zero value.  Therefore
       * the '>' instead of '>='
       *
       * msg_ref == 0 means that the user didn't specify msg_ref
       * msg_ref >= 1 means that the user supplied the given msg_ref, 1 is equal to @0 */
      if (e->msg_ref > num_messages)
        {
          /* msg_ref out of range, we expand to empty string without evaluating the element */
          t = LM_VT_STRING;
          continue;
        }
      msg_ndx = num_messages - e->msg_ref;

      /* value and macro can't understand a context, assume that no msg_ref means @0 */
      if (e->msg_ref == 0)
        msg_ndx--;

      if (escape)
        g_string_truncate(target_buffer, 0);

      switch (e->type)
        {
        case LTE_VALUE:
          log_template_append_elem_value(self, e, options, messages[msg_ndx], &t, target_buffer);
          break;
        case LTE_MACRO:
          log_template_append_elem_macro(self, e, options, messages[msg_ndx], &t, target_buffer);
          break;
        case LTE_FUNC:
          log_template_append_elem_func(self, e, options, messages, num_messages, msg_ndx, &t, target_buffer);
          break;
        default:
          g_assert_not_reached();
          break;
        }

      if (escape)
        {
          if (options->escape)
            options->escape(result, target_buffer->str, target_buffer->len);
          else
            log_template_default_escape_method(result, target_buffer->str, target_buffer->len);
          t = LM_VT_STRING;
        }
    }
  if (type)
    {
      if (first_elem && t == LM_VT_NONE)
        {
          /* empty template string, use LM_VT_STRING before applying the type-cast */
          t = LM_VT_STRING;
        }
      /* apply cast */
      t = _propagate_type(self->type_hint, t);

      /* default to string if none */
      *type = _propagate_type(t, LM_VT_STRING);
    }
}

void
log_template_append_format_with_context(LogTemplate *self, LogMessage **messages, gint num_messages,
                                        LogTemplateEvalOptions *options,
                                        GString *result)
{
  log_template_append_format_value_and_type_with_context(self, messages, num_messages, options, result, NULL);
}

void
log_template_format_with_context(LogTemplate *self, LogMessage **messages, gint num_messages,
                                 LogTemplateEvalOptions *options, GString *result)
{
  g_string_truncate(result, 0);
  log_template_append_format_with_context(self, messages, num_messages, options, result);
}

void
log_template_append_format_value_and_type(LogTemplate *self, LogMessage *lm, LogTemplateEvalOptions *options,
                                          GString *result, LogMessageValueType *type)
{
  log_template_append_format_value_and_type_with_context(self, &lm, 1, options, result, type);
}

void
log_template_append_format(LogTemplate *self, LogMessage *lm, LogTemplateEvalOptions *options, GString *result)
{
  log_template_append_format_value_and_type(self, lm, options, result, NULL);
}

void
log_template_format_value_and_type(LogTemplate *self, LogMessage *lm, LogTemplateEvalOptions *options,
                                   GString *result, LogMessageValueType *type)
{
  g_string_truncate(result, 0);
  log_template_append_format_value_and_type(self, lm, options, result, type);
}

void
log_template_format_value_and_type_with_context(LogTemplate *self, LogMessage **messages, gint num_messages,
                                                LogTemplateEvalOptions *options,
                                                GString *result, LogMessageValueType *type)
{
  g_string_truncate(result, 0);
  log_template_append_format_value_and_type_with_context(self, messages, num_messages, options, result, type);
}

void
log_template_format(LogTemplate *self, LogMessage *lm, LogTemplateEvalOptions *options, GString *result)
{
  log_template_format_value_and_type(self, lm, options, result, NULL);
}

guint
log_template_hash(LogTemplate *self, LogMessage *lm, LogTemplateEvalOptions *options)
{
  if (log_template_is_literal_string(self))
    return g_str_hash(log_template_get_literal_value(self, NULL));

  if (log_template_is_trivial(self))
    {
      NVHandle handle = log_template_get_trivial_value_handle(self);
      g_assert(handle != LM_V_NONE);
      return g_str_hash(log_msg_get_value(lm, handle, NULL));
    }

  ScratchBuffersMarker mark;
  GString *buffer = scratch_buffers_alloc_and_mark(&mark);
  log_template_format(self, lm, options, buffer);
  guint hash = g_str_hash(buffer->str);
  scratch_buffers_reclaim_marked(mark);

  return hash;
}
