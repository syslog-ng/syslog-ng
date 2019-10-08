/*
 * Copyright (c) 2019 Balazs Scheidler <bazsi77@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "add-contextual-data-glob-selector.h"
#include "scratch-buffers.h"
#include "messages.h"

typedef struct _GlobExpression
{
  gchar *pattern;
  GPatternSpec *expr;
} GlobExpression;

static void
glob_expression_populate(GlobExpression *gs, const gchar *pattern)
{
  gs->pattern = g_strdup(pattern);
  gs->expr = g_pattern_spec_new(pattern);
}

static void
glob_expression_clear(GlobExpression *gs)
{
  g_free(gs->pattern);
  g_pattern_spec_free(gs->expr);
}

typedef struct _AddContextualDataGlobSelector
{
  AddContextualDataSelector super;
  GArray *globs;
  LogTemplate *glob_template;
} AddContextualDataGlobSelector;

static GArray *
_create_globs_array(void)
{
  GArray *array = g_array_new(FALSE, TRUE, sizeof(GlobExpression));
  g_array_set_clear_func(array, (void (*)(void *)) glob_expression_clear);
  return array;
}

static void
_populate_globs_array(AddContextualDataGlobSelector *self, GList *ordered_selectors)
{
  for (GList *l = ordered_selectors; l; l = l->next)
    {
      const gchar *selector = (const gchar *) l->data;
      GlobExpression gs;

      glob_expression_populate(&gs, selector);
      g_array_append_val(self->globs, gs);
    }
}

static GArray *
_clone_globs_array(GArray *src)
{
  GArray *dst = _create_globs_array();

  for (gint i = 0; i < src->len; i++)
    {
      GlobExpression *src_expr = &g_array_index(src, GlobExpression, i);
      GlobExpression dst_expr;

      glob_expression_populate(&dst_expr, src_expr->pattern);
      g_array_append_val(dst, dst_expr);
    }
  return dst;
}

static const gchar *
_find_first_matching_glob(AddContextualDataGlobSelector *self, LogMessage *msg)
{
  GString *string = scratch_buffers_alloc();
  GString *string_reversed = scratch_buffers_alloc();

  log_template_format(self->glob_template, msg, NULL, LTZ_LOCAL, 0, NULL, string);
  g_string_assign(string_reversed, string->str);
  g_strreverse(string_reversed->str);
  for (gint i = 0; i < self->globs->len; i++)
    {
      GlobExpression *gs = &g_array_index(self->globs, GlobExpression, i);
      gboolean result = g_pattern_match(gs->expr, string->len, string->str, string_reversed->str);

      msg_trace("add-contextual-data(): Evaluating glob against message",
                evt_tag_str("glob-template", self->glob_template->template),
                evt_tag_str("string", string->str),
                evt_tag_str("pattern", gs->pattern),
                evt_tag_int("matched", result));
      if (result)
        return gs->pattern;
    }

  return NULL;
}

static gboolean
_init(AddContextualDataSelector *s, GList *ordered_selectors)
{
  AddContextualDataGlobSelector *self = (AddContextualDataGlobSelector *)s;

  _populate_globs_array(self, ordered_selectors);

  return TRUE;
}

static gchar *
_resolve(AddContextualDataSelector *s, LogMessage *msg)
{
  AddContextualDataGlobSelector *self = (AddContextualDataGlobSelector *)s;

  return g_strdup(_find_first_matching_glob(self, msg));
}

static void
_free(AddContextualDataSelector *s)
{
  AddContextualDataGlobSelector *self = (AddContextualDataGlobSelector *)s;

  log_template_unref(self->glob_template);
  g_array_free(self->globs, TRUE);
}

static AddContextualDataSelector *_clone(AddContextualDataSelector *s,
                                         GlobalConfig *cfg);

static void
add_contextual_data_glob_selector_init_instance(AddContextualDataGlobSelector *self, LogTemplate *glob_template)
{
  self->super.ordering_required = TRUE;
  self->super.resolve = _resolve;
  self->super.free = _free;
  self->super.init = _init;
  self->super.clone = _clone;
  self->glob_template = glob_template;
}

static AddContextualDataSelector *
_clone(AddContextualDataSelector *s, GlobalConfig *cfg)
{
  AddContextualDataGlobSelector *self = (AddContextualDataGlobSelector *) s;
  AddContextualDataGlobSelector *cloned = g_new0(AddContextualDataGlobSelector, 1);

  add_contextual_data_glob_selector_init_instance(cloned, log_template_ref(self->glob_template));
  cloned->globs = _clone_globs_array(self->globs);
  return &cloned->super;
}

AddContextualDataSelector *
add_contextual_data_glob_selector_new(LogTemplate *glob_template)
{
  AddContextualDataGlobSelector *self = g_new0(AddContextualDataGlobSelector, 1);

  add_contextual_data_glob_selector_init_instance(self, glob_template);
  self->globs = _create_globs_array();
  return &self->super;
}
