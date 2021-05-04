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
#include "template/repr.h"
#include "cfg.h"

gboolean
log_template_is_literal_string(const LogTemplate *self)
{
  if (!self->compiled_template)
    return TRUE;

  if (self->escape || self->compiled_template->next)
    return FALSE;

  return log_template_elem_is_literal_string((LogTemplateElem *) self->compiled_template->data);
}

const gchar *
log_template_get_literal_value(const LogTemplate *self, gssize *value_len)
{
  g_assert(log_template_is_literal_string(self));

  if (!self->compiled_template)
    return "";

  LogTemplateElem *e = (LogTemplateElem *) self->compiled_template->data;

  if (value_len)
    *value_len = e->text_len;

  return e->text;
}

gboolean
log_template_is_trivial(LogTemplate *self)
{
  return self->trivial;
}

const gchar *
log_template_get_trivial_value(LogTemplate *self, LogMessage *msg, gssize *value_len)
{
  g_assert(self->trivial);

  if (!self->compiled_template)
    return "";

  LogTemplateElem *e = (LogTemplateElem *) self->compiled_template->data;

  switch (e->type)
    {
    case LTE_MACRO:
      if (e->text_len > 0)
        {
          if (value_len)
            *value_len = e->text_len;
          return e->text;
        }
      else if (e->macro == M_MESSAGE)
        return log_msg_get_value(msg, LM_V_MESSAGE, value_len);
      else if (e->macro == M_HOST)
        return log_msg_get_value(msg, LM_V_HOST, value_len);
      g_assert_not_reached();
    case LTE_VALUE:
      return log_msg_get_value(msg, e->value_handle, value_len);
    default:
      g_assert_not_reached();
    }
}

static gboolean
_calculate_triviality(LogTemplate *self)
{
  /* if we need to escape, that's not trivial */
  if (self->escape)
    return FALSE;

  /* empty templates are trivial */
  if (self->compiled_template == NULL)
    return TRUE;

  /* more than one element */
  if (self->compiled_template->next != NULL)
    return FALSE;

  const LogTemplateElem *e = (LogTemplateElem *) self->compiled_template->data;

  /* reference to non-last element of the context, that's not trivial */
  if (e->msg_ref > 0)
    return FALSE;

  if (log_template_elem_is_literal_string(e))
    return TRUE;

  switch (e->type)
    {
    case LTE_FUNC:
      /* functions are never trivial */
      return FALSE;
    case LTE_MACRO:
      if (e->text_len > 0)
        return FALSE;

      /* we have macros for MESSAGE and HOST for compatibility reasons, but
       * they should be considered trivial */

      if (e->macro == M_MESSAGE || e->macro == M_HOST)
        return TRUE;
      return FALSE;
    case LTE_VALUE:
      /* values are trivial if they don't contain text */
      return e->text_len == 0;
    default:
      g_assert_not_reached();
    }
}

static void
log_template_reset_compiled(LogTemplate *self)
{
  log_template_elem_free_list(self->compiled_template);
  self->compiled_template = NULL;
  self->trivial = FALSE;
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

  self->trivial = _calculate_triviality(self);
  return result;
}

void
log_template_compile_literal_string(LogTemplate *self, const gchar *literal)
{
  log_template_reset_compiled(self);
  g_free(self->template);
  self->template = g_strdup(literal);
  self->compiled_template = g_list_append(self->compiled_template,
                                          log_template_elem_new_macro(literal, M_NONE, NULL, 0));

  self->trivial = _calculate_triviality(self);
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
  g_atomic_counter_set(&self->ref_cnt, 1);
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
    g_atomic_counter_inc(&s->ref_cnt);
  return s;
}

void
log_template_unref(LogTemplate *s)
{
  if (s && g_atomic_counter_dec_and_test(&s->ref_cnt))
    log_template_free(s);
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
  options->use_fqdn = cfg->host_resolve_options.use_fqdn;
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
  options->use_fqdn = FALSE;
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
