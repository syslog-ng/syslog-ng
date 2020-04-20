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

#include "template/repr.h"

LogTemplateElem *
log_template_elem_new_macro(const gchar *text, guint macro, gchar *default_value, gint msg_ref)
{
  LogTemplateElem *e;

  e = g_new0(LogTemplateElem, 1);
  e->type = LTE_MACRO;
  e->text_len = strlen(text);
  e->text = g_strdup(text);
  e->macro = macro;
  e->default_value = default_value;
  e->msg_ref = msg_ref;
  return e;
}

LogTemplateElem *
log_template_elem_new_value(const gchar *text, gchar *value_name, gchar *default_value, gint msg_ref)
{
  LogTemplateElem *e;

  e = g_new0(LogTemplateElem, 1);
  e->type = LTE_VALUE;
  e->text_len = strlen(text);
  e->text = g_strdup(text);
  e->value_handle = log_msg_get_value_handle(value_name);
  e->default_value = default_value;
  e->msg_ref = msg_ref;
  return e;
}

static gboolean
_setup_function_call(LogTemplate *template, Plugin *p, LogTemplateElem *e,
                     gint argc, gchar *argv[],
                     GError **error)
{
  gchar *argv_copy[argc + 1];

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  e->func.ops = plugin_construct(p);
  e->func.state = e->func.ops->size_of_state > 0 ? g_malloc0(e->func.ops->size_of_state) : NULL;

  /* prepare may modify the argv array: remove and rearrange elements */
  memcpy(argv_copy, argv, (argc + 1) * sizeof(argv[0]));
  if (!e->func.ops->prepare(e->func.ops, e->func.state, template, argc, argv_copy, error))
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
  return TRUE;
}

static gboolean
_lookup_and_setup_function_call(LogTemplate *template, LogTemplateElem *e,
                                gint argc, gchar *argv[],
                                GError **error)
{
  Plugin *p;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  /* the plus one denotes the function name, which'll be removed from argc
   * during parsing */

  if (argc > TEMPLATE_INVOKE_MAX_ARGS + 1)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "Too many arguments (%d) to template function \"%s\", "
                  "maximum number of arguments is %d", argc - 1, argv[0],
                  TEMPLATE_INVOKE_MAX_ARGS);
      goto error;
    }

  p = cfg_find_plugin(template->cfg, LL_CONTEXT_TEMPLATE_FUNC, argv[0]);

  if (!p)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "Unknown template function \"%s\"", argv[0]);
      goto error;
    }

  if (!_setup_function_call(template, p, e, argc, argv, error))
    goto error;

  return TRUE;
error:
  return FALSE;
}

/* NOTE: this steals argv if successful */
LogTemplateElem *
log_template_elem_new_func(LogTemplate *template,
                           const gchar *text, gint argc, gchar *argv[], gint msg_ref,
                           GError **error)
{
  LogTemplateElem *e;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  e = g_malloc0(sizeof(LogTemplateElem) + (argc - 1) * sizeof(LogTemplate *));
  e->type = LTE_FUNC;
  e->text_len = strlen(text);
  e->text = g_strdup(text);
  e->msg_ref = msg_ref;

  if (!_lookup_and_setup_function_call(template, e, argc, argv, error))
    goto error;
  return e;

error:
  if (e->text)
    g_free(e->text);
  g_free(e);
  return NULL;
}


void
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
    default:
      break;
    }
  if (e->default_value)
    g_free(e->default_value);
  if (e->text)
    g_free(e->text);
  g_free(e);
}

void
log_template_elem_free_list(GList *l)
{
  GList *el = l;

  for (; el; el = el->next)
    {
      log_template_elem_free((LogTemplateElem *) el->data);
    }
  g_list_free(l);
}
