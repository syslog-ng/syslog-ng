/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#include "template/templates.h"
#include "template/repr.h"
#include "template/compiler.h"
#include "template/macros.h"
#include "template/escaping.h"
#include "cfg.h"

static void
log_template_reset_compiled(LogTemplate *self)
{
  log_template_elem_free_list(self->compiled_template);
  self->compiled_template = NULL;
}

gboolean
log_template_compile(LogTemplate *self, const gchar *template, GError **error)
{
  LogTemplateCompiler compiler;
  gboolean result;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  log_template_reset_compiled(self);
  if (self->template)
    g_free(self->template);
  self->template = g_strdup(template);

  log_template_compiler_init(&compiler, self);
  result = log_template_compiler_compile(&compiler, &self->compiled_template, error);
  log_template_compiler_clear(&compiler);
  return result;
}

void
log_template_set_escape(LogTemplate *self, gboolean enable)
{
  self->escape = enable;
}

gboolean
log_template_set_type_hint(LogTemplate *self, const gchar *type_hint, GError **error)
{
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  return type_hint_parse(type_hint, &self->type_hint, error);
}


void
log_template_append_format_with_context(LogTemplate *self, LogMessage **messages, gint num_messages,
                                        const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result)
{
  GList *p;
  LogTemplateElem *e;

  if (!opts)
    opts = &self->cfg->template_options;

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
              log_macro_expand(result, e->macro, self->escape, opts ? opts : &self->cfg->template_options, tz, seq_num, context_id,
                               messages[msg_ndx]);
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
                opts,
                tz,
                seq_num,
                context_id
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
                                 const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result)
{
  g_string_truncate(result, 0);
  log_template_append_format_with_context(self, messages, num_messages, opts, tz, seq_num, context_id, result);
}

void
log_template_append_format(LogTemplate *self, LogMessage *lm, const LogTemplateOptions *opts, gint tz, gint32 seq_num,
                           const gchar *context_id, GString *result)
{
  log_template_append_format_with_context(self, &lm, 1, opts, tz, seq_num, context_id, result);
}

void
log_template_format(LogTemplate *self, LogMessage *lm, const LogTemplateOptions *opts, gint tz, gint32 seq_num,
                    const gchar *context_id, GString *result)
{
  g_string_truncate(result, 0);
  log_template_append_format(self, lm, opts, tz, seq_num, context_id, result);
}


/* NOTE: we should completely get rid off the name property of templates,
 * we basically use it at two locations:
 *
 *   1) dbparser uses it to store SyntheticMessages, there the "name" of a
 *      value is stored here
 *
 *   2) we reuse the LogTemplate structure (which represents a compiled
 *      template) to store a template {} statement in the configuration,
 *      which apart from a compiled template, also sports a name.  This was
 *      the original reason the name attribute was introduced. This basically
 *      blends two unrelated purposes in the same struct.
 *
 * Other call sites pass a dummy value, which is probably never used in any
 * meaningful way.
 *
 * Both usages and the dummy call-sites should be removed, and the entire
 * thing replaced by another struct that contains a LogTemplate.
 *
 * I saw this to cause confusion numerous times already.
 * --
 * Bazsi.
 */
void
log_template_set_name(LogTemplate *self, const gchar *name)
{
  if (self->name)
    g_free(self->name);
  self->name = g_strdup(name);
}

/* NOTE: the name parameter should not be used, please pass a NULL until it is eliminated */
LogTemplate *
log_template_new(GlobalConfig *cfg, const gchar *name)
{
  LogTemplate *self = g_new0(LogTemplate, 1);

  log_template_set_name(self, name);
  self->ref_cnt = 1;
  self->cfg = cfg;
  return self;
}

static void
log_template_free(LogTemplate *self)
{
  log_template_reset_compiled(self);
  g_free(self->name);
  g_free(self->template);
  g_free(self);
}

LogTemplate *
log_template_ref(LogTemplate *s)
{
  if (s)
    {
      g_assert(s->ref_cnt > 0);
      s->ref_cnt++;
    }
  return s;
}

void
log_template_unref(LogTemplate *s)
{
  if (s)
    {
      g_assert(s->ref_cnt > 0);
      if (--s->ref_cnt == 0)
        log_template_free(s);
    }
}

/* NOTE: _init needs to be idempotent when called multiple times w/o invoking _destroy */
void
log_template_options_init(LogTemplateOptions *options, GlobalConfig *cfg)
{
  gint i;

  if (options->initialized)
    return;
  if (options->ts_format == -1)
    options->ts_format = cfg->template_options.ts_format;
  for (i = 0; i < LTZ_MAX; i++)
    {
      if (options->time_zone[i] == NULL)
        options->time_zone[i] = g_strdup(cfg->template_options.time_zone[i]);
      if (options->time_zone_info[i] == NULL)
        options->time_zone_info[i] = time_zone_info_new(options->time_zone[i]);
    }

  if (options->frac_digits == -1)
    options->frac_digits = cfg->template_options.frac_digits;
  if (options->on_error == -1)
    options->on_error = cfg->template_options.on_error;
  options->initialized = TRUE;
}

void
log_template_options_destroy(LogTemplateOptions *options)
{
  gint i;

  for (i = 0; i < LTZ_MAX; i++)
    {
      if (options->time_zone[i])
        g_free(options->time_zone[i]);
      if (options->time_zone_info[i])
        time_zone_info_free(options->time_zone_info[i]);
    }
  options->initialized = FALSE;
}

void
log_template_options_defaults(LogTemplateOptions *options)
{
  memset(options, 0, sizeof(LogTemplateOptions));
  options->frac_digits = -1;
  options->ts_format = -1;
  options->on_error = -1;
}

GQuark
log_template_error_quark(void)
{
  return g_quark_from_static_string("log-template-error-quark");
}

void
log_template_global_init(void)
{
  log_macros_global_init();
}

void
log_template_global_deinit(void)
{
  log_macros_global_deinit();
}

gboolean
log_template_on_error_parse(const gchar *strictness, gint *out)
{
  const gchar *p = strictness;
  gboolean silently = FALSE;

  if (!strictness)
    {
      *out = ON_ERROR_DROP_MESSAGE;
      return TRUE;
    }

  if (strncmp(strictness, "silently-", strlen("silently-")) == 0)
    {
      silently = TRUE;
      p = strictness + strlen("silently-");
    }

  if (strcmp(p, "drop-message") == 0)
    *out = ON_ERROR_DROP_MESSAGE;
  else if (strcmp(p, "drop-property") == 0)
    *out = ON_ERROR_DROP_PROPERTY;
  else if (strcmp(p, "fallback-to-string") == 0)
    *out = ON_ERROR_FALLBACK_TO_STRING;
  else
    return FALSE;

  if (silently)
    *out |= ON_ERROR_SILENT;

  return TRUE;
}

void
log_template_options_set_on_error(LogTemplateOptions *options, gint on_error)
{
  options->on_error = on_error;
}
