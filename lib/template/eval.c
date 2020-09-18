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

void
log_template_append_format_with_context(LogTemplate *self, LogMessage **messages, gint num_messages,
                                        LogTemplateEvalOptions *options, GString *result)
{
  GList *p;
  LogTemplateElem *e;

  if (!options->opts)
    options->opts = &self->cfg->template_options;

  for (p = self->compiled_template; p; p = g_list_next(p))
    {
      gint msg_ndx;

      e = (LogTemplateElem *) p->data;
      if (e->text)
        {
          g_string_append_len(result, e->text, e->text_len);
        }

      /* NOTE: msg_ref is 1 larger than the index specified by the user in
       * order to make it distinguishable from the zero value.  Therefore
       * the '>' instead of '>='
       *
       * msg_ref == 0 means that the user didn't specify msg_ref
       * msg_ref >= 1 means that the user supplied the given msg_ref, 1 is equal to @0 */
      if (e->msg_ref > num_messages)
        continue;
      msg_ndx = num_messages - e->msg_ref;

      /* value and macro can't understand a context, assume that no msg_ref means @0 */
      if (e->msg_ref == 0)
        msg_ndx--;

      switch (e->type)
        {
        case LTE_VALUE:
        {
          const gchar *value = NULL;
          gssize value_len = -1;

          value = log_msg_get_value(messages[msg_ndx], e->value_handle, &value_len);
          if (value && value[0])
            result_append(result, value, value_len, self->escape);
          else if (e->default_value)
            result_append(result, e->default_value, -1, self->escape);
          break;
        }
        case LTE_MACRO:
        {
          gint len = result->len;

          if (e->macro)
            {
              log_macro_expand(result, e->macro, self->escape, options, messages[msg_ndx]);
              if (len == result->len && e->default_value)
                g_string_append(result, e->default_value);
            }
          break;
        }
        case LTE_FUNC:
        {
          if (1)
            {
              LogTemplateInvokeArgs args =
              {
                e->msg_ref ? &messages[msg_ndx] : messages,
                e->msg_ref ? 1 : num_messages,
                options,
              };


              /* if a function call is called with an msg_ref, we only
               * pass that given logmsg to argument resolution, otherwise
               * we pass the whole set so the arguments can individually
               * specify which message they want to resolve from
               */
              if (e->func.ops->eval)
                e->func.ops->eval(e->func.ops, e->func.state, &args);
              e->func.ops->call(e->func.ops, e->func.state, &args, result);
            }
          break;
        }
        default:
          g_assert_not_reached();
          break;
        }
    }
}

void
log_template_format_with_context(LogTemplate *self, LogMessage **messages, gint num_messages,
                                 LogTemplateEvalOptions *options, GString *result)
{
  g_string_truncate(result, 0);
  log_template_append_format_with_context(self, messages, num_messages, options, result);
}

void
log_template_append_format(LogTemplate *self, LogMessage *lm, LogTemplateEvalOptions *options, GString *result)
{
  log_template_append_format_with_context(self, &lm, 1, options, result);
}

void
log_template_format(LogTemplate *self, LogMessage *lm, LogTemplateEvalOptions *options, GString *result)
{
  g_string_truncate(result, 0);
  log_template_append_format(self, lm, options, result);
}
