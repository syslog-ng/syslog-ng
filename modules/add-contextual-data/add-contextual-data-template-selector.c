/*
 * Copyright (c) 2016 Balabit
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

#include "add-contextual-data-template-selector.h"
#include "template/templates.h"
#include "syslog-ng.h"
#include "messages.h"

typedef struct _AddContextualDataTemplateSelector
{
  AddContextualDataSelector super;
  LogTemplate *selector_template;
} AddContextualDataTemplateSelector;

static gboolean
_init(AddContextualDataSelector *s, GList *ordered_selectors)
{
  return TRUE;
}

static gchar *
_resolve(AddContextualDataSelector *s, LogMessage *msg)
{
  GString *selector_str = g_string_new(NULL);
  AddContextualDataTemplateSelector *self = (AddContextualDataTemplateSelector *)s;

  log_template_format(self->selector_template, msg, NULL, LTZ_LOCAL, 0, NULL,
                      selector_str);

  return g_string_free(selector_str, FALSE);
}

static void
_free(AddContextualDataSelector *s)
{
  AddContextualDataTemplateSelector *self = (AddContextualDataTemplateSelector *)s;
  log_template_unref(self->selector_template);
  g_free(self);
}

static AddContextualDataSelector *
_clone(AddContextualDataSelector *s, GlobalConfig *cfg)
{
  AddContextualDataTemplateSelector *self = (AddContextualDataTemplateSelector *)s;
  AddContextualDataTemplateSelector *cloned = (AddContextualDataTemplateSelector *)
                                              add_contextual_data_template_selector_new(log_template_ref(self->selector_template));
  return &cloned->super;
}

AddContextualDataSelector *
add_contextual_data_template_selector_new(LogTemplate *selector_template)
{
  AddContextualDataTemplateSelector *new_instance = g_new0(AddContextualDataTemplateSelector, 1);
  new_instance->selector_template = selector_template;
  new_instance->super.resolve = _resolve;
  new_instance->super.free = _free;
  new_instance->super.init = _init;
  new_instance->super.clone = _clone;

  return &new_instance->super;
}
