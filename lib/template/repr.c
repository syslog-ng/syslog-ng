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
