/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2014 Balázs Scheidler
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

#include "template/compiler.h"
#include "template/templates.h"
#include "template/repr.h"
#include "template/macros.h"
#include "plugin.h"

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
  gchar *str;

  e = g_new0(LogTemplateElem, 1);
  e->type = LTE_VALUE;
  e->text_len = self->text ? self->text->len : 0;
  e->text = self->text ? g_strndup(self->text->str, self->text->len) : NULL;

  /* value_name is not NUL terminated */
  str = g_strndup(value_name, value_name_len);
  e->value_handle = log_msg_get_value_handle(str);
  g_free(str);

  e->default_value = default_value;
  e->msg_ref = self->msg_ref;
  self->result = g_list_prepend(self->result, e);
}

static gboolean
log_template_prepare_function_call(LogTemplateCompiler *self, Plugin *p, LogTemplateElem *e, gint argc, gchar *argv[],
                                   GError **error)
{
  gchar *argv_copy[argc + 1];

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  e->func.ops = plugin_construct(p);
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
      if (e->func.ops->free_fn)
        e->func.ops->free_fn(e->func.ops);
      return FALSE;
    }
  g_strfreev(argv);
  self->result = g_list_prepend(self->result, e);
  return TRUE;
}

static gboolean
log_template_lookup_and_setup_function_call(LogTemplateCompiler *self, LogTemplateElem *e, gint argc, gchar *argv[],
                                            GError **error)
{
  Plugin *p;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  p = cfg_find_plugin(self->template->cfg, LL_CONTEXT_TEMPLATE_FUNC, argv[0]);

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
                          evt_tag_str("Template", self->template->template));
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
  gboolean arg_buf_has_a_value = FALSE;
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
          arg_buf_has_a_value = TRUE;
          continue;
        }
      else if (parens == 1 && (*self->cursor == ' ' || *self->cursor == '\t'))
        {
          g_ptr_array_add(result, g_strndup(arg_buf->str, arg_buf->len));
          g_string_truncate(arg_buf, 0);
          arg_buf_has_a_value = FALSE;
          while (*self->cursor && (*self->cursor == ' ' || *self->cursor == '\t'))
            self->cursor++;
          continue;
        }
      log_template_compiler_append_and_increment(self, arg_buf);
      arg_buf_has_a_value = TRUE;
    }
  if (arg_buf_has_a_value)
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
      log_template_compiler_fill_compile_error(error,
                                               "Invalid template function reference, missing function name or imbalanced '('",
                                               self->cursor - self->template->template);
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
                      evt_tag_str("Template", self->template->template));
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

gboolean
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

void
log_template_compiler_init(LogTemplateCompiler *self, LogTemplate *template)
{
  memset(self, 0, sizeof(*self));

  self->template = log_template_ref(template);
  self->cursor = self->template->template;
  self->text = g_string_sized_new(32);
}

void
log_template_compiler_clear(LogTemplateCompiler *self)
{
  log_template_unref(self->template);
  g_string_free(self->text, TRUE);
}
