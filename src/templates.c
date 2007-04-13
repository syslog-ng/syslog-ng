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
          if (last_macro != M_NONE)
            {
              e = g_new0(LogTemplateElem, 1);
              e->macro = last_macro;
              e->text = last_text;
              self->compiled_template = g_list_prepend(self->compiled_template, e);
              last_macro = M_NONE;
              last_text = NULL;
            }
          p++;
          /* macro reference */
          if (*p >= '0' && *p <= '9')
            {
              last_macro = M_MATCH_REF_OFS + (*p - '0');
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
log_template_format(LogTemplate *self, LogMessage *lm, guint macro_flags, GString *result)
{
  GList *p;
  LogTemplateElem *e;
  
  if (self->compiled_template == NULL)
    log_template_compile(self);
  
  for (p = self->compiled_template; p; p = g_list_next(p))
    {
      e = (LogTemplateElem *) p->data;
      if (e->macro != M_NONE)
        {
          log_macro_expand(result, e->macro, 
                           macro_flags |
                           ((self->flags & LT_ESCAPE) ? MF_ESCAPE_RESULT : 0),
                           (self->flags & LT_TZ_SET) ? self->zone_offset : timezone,
                           lm);
        }
      if (e->text)
        {
          g_string_append(result, e->text->str);
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
