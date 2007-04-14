/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "templates.h"
#include "macros.h"

#include <time.h>

typedef struct _LogTemplateElem
{
  guint macro;
  GString *text;
} LogTemplateElem;

void
log_template_set_escape(LogTemplate *self, gboolean enable)
{
  if (enable)
    self->flags |= LT_ESCAPE;
  else
    self->flags &= ~LT_ESCAPE;
}

void
log_template_compile(LogTemplate *self)
{
  gchar *start, *p;
  guint last_macro = M_NONE;
  GString *last_text = NULL;
  LogTemplateElem *e;
  
  p = self->template->str;
  
  while (*p)
    {
      if (*p == '$')
        {
          p++;
          /* macro reference */
          if (*p >= '0' && *p <= '9')
            {
              last_macro = M_MATCH_REF_OFS + (*p - '0');
              p++;
            }
          else if (*p == '{')
            {
              p++;
              start = p;
              while (*p && *p != '}')
                {
                  p++;
                }
              p++;
              last_macro = log_macro_lookup(start, p - start - 1);
            }
          else
            {
              start = p;
              while ((*p >= 'A' && *p <= 'Z') || (*p == '_'))
                {
                  p++;
                }
              last_macro = log_macro_lookup(start, p - start);
            }
          if (last_macro != M_NONE)
            {
              e = g_new0(LogTemplateElem, 1);
              e->macro = last_macro;
              e->text = last_text;
              self->compiled_template = g_list_prepend(self->compiled_template, e);
              last_macro = M_NONE;
              last_text = NULL;
            }
          
        }
      else
        {
          if (last_text == NULL)
            last_text = g_string_sized_new(32);
          g_string_append_c(last_text, *p);
          p++;
        }
    }
  if (last_macro != M_NONE || last_text)
    {
      e = g_new0(LogTemplateElem, 1);
      e->macro = last_macro;
      e->text = last_text;
      self->compiled_template = g_list_prepend(self->compiled_template, e);
    }
  self->compiled_template = g_list_reverse(self->compiled_template);
}

void
log_template_format(LogTemplate *self, LogMessage *lm, guint macro_flags, gint ts_format, glong zone_offset, gint frac_digits, GString *result)
{
  GList *p;
  LogTemplateElem *e;
  
  if (self->compiled_template == NULL)
    log_template_compile(self);

  g_string_truncate(result, 0);
  for (p = self->compiled_template; p; p = g_list_next(p))
    {
      e = (LogTemplateElem *) p->data;
      if (e->text)
        {
          g_string_append(result, e->text->str);
        }
      if (e->macro != M_NONE)
        {
          log_macro_expand(result, e->macro, 
                           macro_flags |
                           ((self->flags & LT_ESCAPE) ? MF_ESCAPE_RESULT : 0),
                           ts_format,
                           zone_offset,
                           frac_digits,
                           lm);
        }
    }
}

LogTemplate *
log_template_new(gchar *name, gchar *template)
{
  LogTemplate *self = g_new0(LogTemplate, 1);
  
  self->name = g_string_new(name);
  self->template = template ? g_string_new(template) : NULL;
  self->flags = LT_ESCAPE;
  self->ref_cnt = 1;
  return self;
}

static void 
log_template_free(LogTemplate *self)
{
  g_string_free(self->name, TRUE);
  g_string_free(self->template, TRUE);
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
