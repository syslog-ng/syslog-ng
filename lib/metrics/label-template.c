/*
 * Copyright (c) 2023 Attila Szakacs
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

#include "label-template.h"

struct _LabelTemplate
{
  gchar *name;
  LogTemplate *value_template;
};

static const gchar *
_format_value(LabelTemplate *self, const LogTemplateOptions *template_options, LogMessage *msg, GString *buffer)
{
  if (log_template_is_trivial(self->value_template))
    {
      gssize len;
      return log_template_get_trivial_value(self->value_template, msg, &len);
    }

  LogTemplateEvalOptions template_eval_options = { template_options, LTZ_SEND, 0, NULL, LM_VT_STRING };
  log_template_format(self->value_template, msg, &template_eval_options, buffer);

  return buffer->str;
}

void
label_template_format(LabelTemplate *self, const LogTemplateOptions *template_options, LogMessage *msg,
                      GString *value_buffer, StatsClusterLabel *label)
{
  label->name = self->name;
  label->value = _format_value(self, template_options, msg, value_buffer);
}

gint
label_template_compare(const LabelTemplate *self, const LabelTemplate *other)
{
  return strcmp(self->name, other->name);
}

LabelTemplate *
label_template_new(const gchar *name, LogTemplate *value_template)
{
  LabelTemplate *self = g_new0(LabelTemplate, 1);

  self->name = g_strdup(name);
  self->value_template = log_template_ref(value_template);

  return self;
}

LabelTemplate *
label_template_clone(const LabelTemplate *self)
{
  return label_template_new(self->name, self->value_template);
}

void
label_template_free(LabelTemplate *self)
{
  g_free(self->name);
  log_template_unref(self->value_template);

  g_free(self);
}
