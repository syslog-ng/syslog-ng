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
#include "timeutils/format.h"
#include "cfg.h"

gboolean
log_template_is_literal_string(const LogTemplate *self)
{
  return self->literal;
}

const gchar *
log_template_get_literal_value(const LogTemplate *self, gssize *value_len)
{
  g_assert(self->literal);

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

NVHandle
log_template_get_trivial_value_handle(LogTemplate *self)
{
  g_assert(self->trivial);

  if (self->literal)
    return LM_V_NONE;

  LogTemplateElem *e = (LogTemplateElem *) self->compiled_template->data;

  switch (e->type)
    {
    case LTE_MACRO:
      if (e->macro == M_MESSAGE)
        return LM_V_MESSAGE;
      else if (e->macro == M_HOST)
        return LM_V_HOST;
      else
        g_assert_not_reached();
      break;
    case LTE_VALUE:
      return e->value_handle;
    default:
      g_assert_not_reached();
    }
}

const gchar *
log_template_get_trivial_value_and_type(LogTemplate *self, LogMessage *msg, gssize *value_len,
                                        LogMessageValueType *type)
{
  LogMessageValueType t = LM_VT_STRING;
  const gchar *result = "";
  gssize result_len = 0;

  g_assert(self->trivial);

  if (self->literal)
    {
      result = log_template_get_literal_value(self, &result_len);
    }
  else
    {
      NVHandle handle = log_template_get_trivial_value_handle(self);
      g_assert(handle != LM_V_NONE);

      result = log_msg_get_value_with_type(msg, handle, &result_len, &t);
    }

  if (type)
    {
      *type = self->type_hint == LM_VT_NONE ? t : self->type_hint;
    }
  if (value_len)
    *value_len = result_len;
  return result;
}

const gchar *
log_template_get_trivial_value(LogTemplate *self, LogMessage *msg, gssize *value_len)
{
  return log_template_get_trivial_value_and_type(self, msg, value_len, NULL);
}

static gboolean
_calculate_if_literal(LogTemplate *self)
{
  if (!self->compiled_template)
    return TRUE;

  if (self->compiled_template->next)
    return FALSE;

  return log_template_elem_is_literal_string((LogTemplateElem *) self->compiled_template->data);
}

static gboolean
_calculate_if_trivial(LogTemplate *self)
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
  if (self->template_str)
    g_free(self->template_str);
  self->template_str = g_strdup(template);

  log_template_compiler_init(&compiler, self);
  result = log_template_compiler_compile(&compiler, &self->compiled_template, error);
  log_template_compiler_clear(&compiler);

  self->literal = _calculate_if_literal(self);
  self->trivial = _calculate_if_trivial(self);
  return result;
}

void
log_template_forget_template_string(LogTemplate *self)
{
  g_free(self->template_str);
  self->template_str = NULL;
}

static void
_split_type_and_template(gchar *spec, gchar **value, gchar **type)
{
  char *sp, *ep;

  *type = NULL;
  sp = spec;

  while (g_ascii_isalnum(*sp) || (*sp) == '_')
    sp++;

  while (*sp == ' ' || *sp == '\t')
    sp++;

  if (*sp != '(' ||
      !((g_ascii_toupper(spec[0]) >= 'A' &&
         g_ascii_toupper(spec[0]) <= 'Z') ||
        spec[0] == '_'))
    {
      *value = spec;
      return;
    }

  ep = strrchr(sp, ')');
  if (ep == NULL || ep[1] != '\0')
    {
      *value = spec;
      return;
    }

  *value = sp + 1;
  *type = spec;
  sp[0] = '\0';
  ep[0] = '\0';
}

gboolean
log_template_compile_with_type_hint(LogTemplate *self, const gchar *template_and_typehint, GError **error)
{
  gchar *buf = g_strdup(template_and_typehint);
  gchar *template_string = NULL;
  gchar *typehint_string = NULL;
  gboolean result = FALSE;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  _split_type_and_template(buf, &template_string, &typehint_string);
  if (!log_template_compile(self, template_string, error))
    goto exit;
  if (!log_template_set_type_hint(self, typehint_string, error))
    goto exit;

  result = TRUE;
exit:
  g_free(buf);
  return result;
}

void
log_template_compile_literal_string(LogTemplate *self, const gchar *literal)
{
  log_template_reset_compiled(self);
  g_free(self->template_str);
  self->template_str = g_strdup(literal);
  self->compiled_template = g_list_append(self->compiled_template,
                                          log_template_elem_new_macro(literal, M_NONE, NULL, 0));

  /* double check that the representation here is actually considered trivial. It should be. */
  g_assert(_calculate_if_trivial(self));

  self->literal = TRUE;
  self->trivial = TRUE;
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
  gboolean result;

  if (!type_hint)
    {
      self->explicit_type_hint = LM_VT_NONE;
      result = TRUE;
    }
  else if (!type_hint_parse(type_hint, &self->explicit_type_hint, error))
    {
      self->explicit_type_hint = LM_VT_NONE;
      result = FALSE;
    }
  else
    {
      result = TRUE;
    }

  self->type_hint = self->explicit_type_hint;
  return result;
}

void
log_template_set_type_hint_value(LogTemplate *self, LogMessageValueType type_hint)
{
  self->explicit_type_hint = self->type_hint = type_hint;
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
  if (cfg_is_config_version_older(cfg, VERSION_VALUE_4_0))
    self->type_hint = LM_VT_STRING;
  else
    self->type_hint = LM_VT_NONE;
  self->explicit_type_hint = LM_VT_NONE;
  self->top_level = TRUE;
  return self;
}

LogTemplate *
log_template_new_embedded(GlobalConfig *cfg)
{
  LogTemplate *self = log_template_new(cfg, NULL);
  self->top_level = FALSE;
  return self;
}

static void
log_template_free(LogTemplate *self)
{
  log_template_reset_compiled(self);
  g_free(self->name);
  g_free(self->template_str);
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
log_template_options_clone(LogTemplateOptions *source, LogTemplateOptions *dest)
{
  dest->ts_format = source->ts_format;
  for (gint i = 0; i < LTZ_MAX; i++)
    {
      if (source->time_zone[i])
        dest->time_zone[i] = g_strdup(source->time_zone[i]);
    }
  dest->frac_digits = source->frac_digits;
  dest->on_error = source->on_error;
  dest->use_fqdn = source->use_fqdn;

  /* NOTE: this still needs to be initialized by the owner as clone results
   * in an uninitialized state.  */
  dest->initialized = FALSE;
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

void
log_template_options_global_defaults(LogTemplateOptions *options)
{
  log_template_options_defaults(options);
  options->ts_format = TS_FMT_BSD;
  options->frac_digits = 0;
  options->on_error = ON_ERROR_DROP_MESSAGE;
}

GQuark
log_template_error_quark(void)
{
  return g_quark_from_static_string("log-template-error-quark");
}

gboolean
log_template_on_error_parse(const gchar *strictness, gint *out)
{
  return on_error_parse(strictness, out);
}

void
log_template_options_set_on_error(LogTemplateOptions *options, gint on_error)
{
  options->on_error = on_error;
}

EVTTAG *
evt_tag_template(const gchar *name, LogTemplate *template_obj, LogMessage *msg, LogTemplateEvalOptions *options)
{
  /* trying to avoid scratch-buffers here, this is only meant to be used in trace messages */
  GString *buf = g_string_sized_new(256);

  log_template_format(template_obj, msg, options, buf);

  EVTTAG *result = evt_tag_str(name, buf->str);
  g_string_free(buf, TRUE);
  return result;
}
