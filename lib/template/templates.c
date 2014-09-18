/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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
#include "template/macros.h"
#include "template/function.h"
#include "messages.h"
#include "logmsg.h"
#include "syslog-names.h"
#include "messages.h"
#include "hostname.h"
#include "filter/filter-expr.h"
#include "gsocket.h"
#include "plugin.h"
#include "plugin-types.h"
#include "str-format.h"
#include "rcptid.h"
#include "run-id.h"

#include <time.h>
#include <string.h>


typedef struct
{
  LogTemplate *template;
  GList *result;
  gchar *cursor;
  GString *text;
  gint msg_ref;
} LogTemplateCompiler;

enum
{
  LTE_MACRO,
  LTE_VALUE,
  LTE_FUNC
};

typedef struct _LogTemplateElem
{
  gsize text_len;
  gchar *text;
  gchar *default_value;
  guint16 msg_ref;
  guint8 type;
  union
  {
    guint macro;
    NVHandle value_handle;
    struct
    {
      LogTemplateFunction *ops;
      gpointer state;
    } func;
  };
} LogTemplateElem;


static void
log_template_add_macro_elem(LogTemplateCompiler *self, guint macro, gchar *default_value)
{
  LogTemplateElem *e;

  e = g_new0(LogTemplateElem, 1);
  e->type = LTE_MACRO;
  e->text_len = self->text ? self->text->len : 0;
  e->text = self->text ? g_strndup(self->text->str, self->text->len) : NULL;
  e->macro = macro;
  e->default_value = default_value;
  e->msg_ref = self->msg_ref;
  self->result = g_list_prepend(self->result, e);
}

static void
log_template_add_value_elem(LogTemplateCompiler *self, gchar *value_name, gsize value_name_len, gchar *default_value)
{
  LogTemplateElem *e;
  gchar *dup;

  e = g_new0(LogTemplateElem, 1);
  e->type = LTE_VALUE;
  e->text_len = self->text ? self->text->len : 0;
  e->text = self->text ? g_strndup(self->text->str, self->text->len) : NULL;
  /* value_name is not NUL terminated */
  dup = g_strndup(value_name, value_name_len);
  e->value_handle = log_msg_get_value_handle(dup);
  g_free(dup);
  e->default_value = default_value;
  e->msg_ref = self->msg_ref;
  self->result = g_list_prepend(self->result, e);
}

static gboolean
log_template_prepare_function_call(LogTemplateCompiler *self, Plugin *p, LogTemplateElem *e, gint argc, gchar *argv[], GError **error)
{
  gchar *argv_copy[argc + 1];

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  e->func.ops = plugin_construct(p, self->template->cfg, LL_CONTEXT_TEMPLATE_FUNC, argv[0]);
  e->func.state = e->func.ops->size_of_state > 0 ? g_malloc0(e->func.ops->size_of_state) : NULL;

  /* prepare may modify the argv array: remove and rearrange elements */
  memcpy(argv_copy, argv, (argc + 1) * sizeof(argv[0]));
  if (!e->func.ops->prepare(e->func.ops, e->func.state, self->template, argc, argv_copy, error))
    {
      if (e->func.state)
        {
          e->func.ops->free_state(e->func.state);
          g_free(e->func.state);
        }
      return FALSE;
    }
  g_strfreev(argv);
  self->result = g_list_prepend(self->result, e);
  return TRUE;
}

static gboolean
log_template_lookup_and_setup_function_call(LogTemplateCompiler *self, LogTemplateElem *e, gint argc, gchar *argv[], GError **error)
{
  Plugin *p;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  p = plugin_find(self->template->cfg, LL_CONTEXT_TEMPLATE_FUNC, argv[0]);

  if (!p)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "Unknown template function \"%s\"", argv[0]);
      goto error;
    }

  if (!log_template_prepare_function_call(self, p, e, argc, argv, error))
    goto error;

  return TRUE;
 error:
  return FALSE;
}


/* NOTE: this steals argv if successful */
static gboolean
log_template_add_func_elem(LogTemplateCompiler *self, gint argc, gchar *argv[], GError **error)
{
  LogTemplateElem *e;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  if (argc == 0)
    return TRUE;

  e = g_malloc0(sizeof(LogTemplateElem) + (argc - 1) * sizeof(LogTemplate *));
  e->type = LTE_FUNC;
  e->text_len = self->text ? self->text->len : 0;
  e->text = self->text ? g_strndup(self->text->str, self->text->len) : NULL;
  e->msg_ref = self->msg_ref;

  if (!log_template_lookup_and_setup_function_call(self, e, argc, argv, error))
    goto error;
  return TRUE;

 error:
  if (e->text)
    g_free(e->text);
  g_free(e);
  return FALSE;
}

static void
log_template_elem_free(LogTemplateElem *e)
{
  switch (e->type)
    {
    case LTE_FUNC:
      if (e->func.state)
        {
          e->func.ops->free_state(e->func.state);
          g_free(e->func.state);
        }
      if (e->func.ops && e->func.ops->free_fn)
        e->func.ops->free_fn(e->func.ops);
      break;
    }
  if (e->default_value)
    g_free(e->default_value);
  if (e->text)
    g_free(e->text);
  g_free(e);
}

static void
log_template_elem_free_list(GList *el)
{
  for (; el; el = el->next)
    {
      log_template_elem_free((LogTemplateElem *) el->data);
    }
  g_list_free(el);
}

static void
parse_msg_ref(LogTemplateCompiler *self)
{
  self->msg_ref = 0;
  if ((*self->cursor) == '@')
    {
      self->cursor++;
      if ((*self->cursor) >= '0' && (*self->cursor) <= '9')
        {
          /* syntax: ${name}@1 to denote the log message index in the correllation state */
          while ((*self->cursor) >= '0' && (*self->cursor) <= '9')
            {
              self->msg_ref += self->msg_ref * 10 + ((*self->cursor) - '0');
              self->cursor++;
            }
          self->msg_ref += 1;
        }
      else
        {
          if ((*self->cursor) != '@')
            {
              msg_warning("Non-numeric correlation state ID found, assuming a literal '@' character. To avoid confusion when using a literal '@' after a macro or template function, write '@@' in the template.",
                          evt_tag_str("Template", self->template->template),
                          NULL);
              self->cursor--;
            }
          self->msg_ref = 0;
        }
    }
}

static void
log_template_compiler_fill_compile_error(GError **error, const gchar *error_info, gint error_pos)
{
  g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "%s, error_pos='%d'", error_info, error_pos);
}

static void
log_template_compiler_append_and_increment(LogTemplateCompiler *self, GString *text)
{
  g_string_append_c(text, *self->cursor);
  self->cursor++;
}

static gint
log_template_compiler_get_macro_length(gchar *start, gchar *end, gchar **token)
{
  gint result = 0;
  gchar *colon = memchr(start, ':', end - start - 1);
  if (colon)
    {
      result = colon - start;
      *token = colon < end ? colon + 1 : NULL;
    }
  else
    {
      result = end - start - 1;
      *token = NULL;
    }
  return result;
}


static gchar *
log_template_compiler_get_default_value(LogTemplateCompiler *self, gchar *token)
{
  g_assert(token);
  if (*token != '-')
    {
      return NULL;
    }
  return g_strndup(token + 1, self->cursor - token - 2);
}

static inline gboolean
is_macro_name(gchar c)
{
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_') || (c >= '0' && c <= '9');
}

#define STEP_BY_TRUE(p, x) while(x) p++;

static void
log_template_compiler_add_elem(LogTemplateCompiler *self, gchar *start, gint macro_len, gchar *default_value)
{
  gint macro = log_macro_lookup(start, macro_len);
  if (macro == M_NONE)
    {
      log_template_add_value_elem(self, start, macro_len, default_value);
    }
  else
    {
      log_template_add_macro_elem(self, macro, default_value);
    }
}

static gboolean
log_template_compiler_process_braced_template(LogTemplateCompiler *self, GError **error)
{
  gint macro_len;
  gchar *default_value = NULL;
  gchar *start;
  gchar *end;
  gchar *token;

  self->cursor++;
  start = self->cursor;
  end = strchr(self->cursor, '}');

  if (!end)
    {
      log_template_compiler_fill_compile_error(error, "Invalid macro, '}' is missing", strlen(self->template->template));
      return FALSE;
    }

  self->cursor = end + 1;
  macro_len = log_template_compiler_get_macro_length(start, self->cursor, &token);
  if (token)
    {
      default_value = log_template_compiler_get_default_value(self, token);
      if (!default_value)
        {
          log_template_compiler_fill_compile_error(error, "Unknown substitution function", token - self->template->template);
          return FALSE;
        }
    }
  parse_msg_ref(self);
  log_template_compiler_add_elem(self, start, macro_len, default_value);
  return TRUE;
}

static gboolean
log_template_compiler_add_quoted_string(LogTemplateCompiler *self, gboolean is_top_level, GString *result)
{
  gchar *quote = self->cursor;
  gchar *end_of_quote = strchr(quote + 1, *quote);
  if (!end_of_quote)
    {
      return FALSE;
    }
  self->cursor = end_of_quote + 1;
  if (is_top_level)
    {
      /* skip the quote in top-level and don't skip in expressions enclosed in parens */
      quote++;
    }
  else
    {
      end_of_quote++;
    }
  g_string_append_len(result, quote, end_of_quote - quote);
  return TRUE;
}

static gboolean
log_template_compiler_process_arg_list(LogTemplateCompiler *self, GPtrArray *result)
{
  GString *arg_buf = g_string_sized_new(32);
  gint parens = 1;
  self->cursor++;

  while (*self->cursor && *self->cursor == ' ')
    self->cursor++;

  while(*self->cursor)
    {
      if (*self->cursor == '\\')
        {
          self->cursor++;
        }
      else if (*self->cursor == '(')
        {
          parens++;
        }
      else if (*self->cursor == ')')
        {
          parens--;
          if (parens == 0)
            {
              break;
            }
        }
      else if (*self->cursor == '"' || *self->cursor == '\'')
        {
          if (!log_template_compiler_add_quoted_string(self, parens == 1, arg_buf))
            {
              g_ptr_array_add(result, NULL);
              g_string_free(arg_buf, TRUE);
              return FALSE;
            }
          continue;
        }
      else if (parens == 1 && (*self->cursor == ' ' || *self->cursor == '\t'))
        {
          g_ptr_array_add(result, g_strndup(arg_buf->str, arg_buf->len));
          g_string_truncate(arg_buf, 0);
          while (*self->cursor && (*self->cursor == ' ' || *self->cursor == '\t'))
            self->cursor++;
          continue;
        }
      log_template_compiler_append_and_increment(self, arg_buf);
    }
  if (arg_buf->len > 0)
    {
      g_ptr_array_add(result, g_strndup(arg_buf->str, arg_buf->len));
    }
  g_ptr_array_add(result, NULL);
  g_string_free(arg_buf, TRUE);
  return *self->cursor == ')';
}

static gboolean
log_template_compiler_process_template_function(LogTemplateCompiler *self, GError **error)
{
  GPtrArray *strv = g_ptr_array_new();

  if (!log_template_compiler_process_arg_list(self, strv))
    {
      log_template_compiler_fill_compile_error(error, "Invalid template function reference, missing function name or inbalanced '('", self->cursor - self->template->template);
      goto error;
    }
  self->cursor++;

  parse_msg_ref(self);
  if (!log_template_add_func_elem(self, strv->len - 1, (gchar **) strv->pdata, error))
    {
      goto error;
    }
  g_ptr_array_free(strv, FALSE);
  return TRUE;
error:
  g_strfreev((gchar **)strv->pdata);
  g_ptr_array_free(strv, FALSE);
  return FALSE;
}

static void
log_template_compiler_process_unbraced_template(LogTemplateCompiler *self)
{
  gchar *start = self->cursor;
  gint macro_len;
  do
    {
      self->cursor++;
    }
  while (is_macro_name(*self->cursor));
  macro_len = self->cursor - start;
  log_template_compiler_add_elem(self, start, macro_len, NULL);
}

static gboolean
log_template_compiler_process_value(LogTemplateCompiler *self, GError **error)
{
  gboolean finished = FALSE;
  gchar p;
  self->cursor++;
  p = *self->cursor;
  /* macro reference */
  if (p == '{')
    {
      if (!log_template_compiler_process_braced_template(self, error))
        {
          return FALSE;
        }
      finished = TRUE;
    }
  /* template function */
  else if (p == '(')
    {
      if (!log_template_compiler_process_template_function(self, error))
        {
          return FALSE;
        }
      finished = TRUE;
    }
  /* unbraced macro */
  else if (is_macro_name(p))
    {
      log_template_compiler_process_unbraced_template(self);
      finished = TRUE;
    }
  /* escaped value with dollar */
  else
    {
      if (p != '$')
        {
          g_string_append_c(self->text, '$');
        }
      if (p)
        {
          log_template_compiler_append_and_increment(self, self->text);
        }
    }
  if (finished)
    {
      g_string_truncate(self->text, 0);
    }
  return TRUE;
}

gboolean
log_template_compiler_process_token(LogTemplateCompiler *self, GError **error)
{
  self->msg_ref = 0;
  if (*self->cursor == '$')
    {
      return log_template_compiler_process_value(self, error);
    }
  if (*self->cursor == '\\')
    {
      if (cfg_is_config_version_older(self->template->cfg, 0x305))
        {
          msg_warning("Template escaping changed in version 3.5. Use '$$' to specify a literal dollar sign instead of '\\$' and remove the escaping of the backslash character when you upgrade your configuration",
                      evt_tag_str("Template", self->template->template),
                      NULL);
          self->cursor++;
        }

    }
  if (*self->cursor)
    {
      log_template_compiler_append_and_increment(self, self->text);
    }
  return TRUE;
}

static void
log_template_compiler_free_result(LogTemplateCompiler *self)
{
  log_template_elem_free_list(self->result);
  self->result = NULL;
}

static gboolean
log_template_compiler_compile(LogTemplateCompiler *self, GList **compiled_template, GError **error)
{
  gboolean result = FALSE;

  while (*self->cursor)
    {
      if (!log_template_compiler_process_token(self, error))
        {
          log_template_compiler_free_result(self);
          g_string_sprintf(self->text, "error in template: %s", self->template->template);
          log_template_add_macro_elem(self, M_NONE, NULL);
          goto error;
        }
    }
  if (self->text->len)
    {
      log_template_add_macro_elem(self, M_NONE, NULL);
    }
  result = TRUE;
 error:
  *compiled_template = g_list_reverse(self->result);
  self->result = NULL;
  return result;
}

static void
log_template_compiler_init(LogTemplateCompiler *self, LogTemplate *template)
{
  memset(self, 0, sizeof(*self));

  self->template = log_template_ref(template);
  self->cursor = self->template->template;
  self->text = g_string_sized_new(32);
}

static void
log_template_compiler_clear(LogTemplateCompiler *self)
{
  log_template_unref(self->template);
  g_string_free(self->text, TRUE);
}

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
log_template_append_format_with_context(LogTemplate *self, LogMessage **messages, gint num_messages, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result)
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
                log_macro_expand(result, e->macro, self->escape, opts ? opts : &self->cfg->template_options, tz, seq_num, context_id, messages[msg_ndx]);
                if (len == result->len && e->default_value)
                  g_string_append(result, e->default_value);
              }
            break;
          }
        case LTE_FUNC:
          {
            g_static_mutex_lock(&self->arg_lock);
            if (!self->arg_bufs)
              self->arg_bufs = g_ptr_array_sized_new(0);

            if (1)
              {
                LogTemplateInvokeArgs args =
                  {
                    self->arg_bufs,
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
            g_static_mutex_unlock(&self->arg_lock);
            break;
          }
        }
    }
}

void
log_template_format_with_context(LogTemplate *self, LogMessage **messages, gint num_messages, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result)
{
  g_string_truncate(result, 0);
  log_template_append_format_with_context(self, messages, num_messages, opts, tz, seq_num, context_id, result);
}

void
log_template_append_format(LogTemplate *self, LogMessage *lm, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result)
{
  log_template_append_format_with_context(self, &lm, 1, opts, tz, seq_num, context_id, result);
}

void
log_template_format(LogTemplate *self, LogMessage *lm, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result)
{
  g_string_truncate(result, 0);
  log_template_append_format(self, lm, opts, tz, seq_num, context_id, result);
}

LogTemplate *
log_template_new(GlobalConfig *cfg, gchar *name)
{
  LogTemplate *self = g_new0(LogTemplate, 1);

  self->name = g_strdup(name);
  self->ref_cnt = 1;
  self->cfg = cfg;
  g_static_mutex_init(&self->arg_lock);
  if (cfg_is_config_version_older(cfg, 0x0300))
    {
      msg_warning_once("WARNING: template: the default value for template-escape has changed to 'no' from " VERSION_3_0 ", please update your configuration file accordingly",
                       NULL);
      self->escape = TRUE;
    }
  return self;
}

static void
log_template_free(LogTemplate *self)
{
  if (self->arg_bufs)
    {
      gint i;

      for (i = 0; i < self->arg_bufs->len; i++)
        g_string_free(g_ptr_array_index(self->arg_bufs, i), TRUE);
      g_ptr_array_free(self->arg_bufs, TRUE);
    }
  log_template_reset_compiled(self);
  g_free(self->name);
  g_free(self->template);
  g_static_mutex_free(&self->arg_lock);
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
log_template_error_quark()
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
