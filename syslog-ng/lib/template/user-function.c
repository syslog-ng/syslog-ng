/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Balázs Scheidler
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

#include "template/function.h"
#include "plugin.h"

typedef struct _UserTemplateFunction
{
  LogTemplateFunction super;
  gchar *name;
  LogTemplate *template;
} UserTemplateFunction;

static gboolean
user_template_function_prepare(LogTemplateFunction *s, gpointer state, LogTemplate *parent, gint argc, gchar *argv[],
                               GError **error)
{
  UserTemplateFunction *self = (UserTemplateFunction *) s;

  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);

  if (argc != 1)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "User defined template function $(%s) cannot have arguments", self->name);
      return FALSE;
    }
  return TRUE;
}

static void
user_template_function_call(LogTemplateFunction *s, gpointer state, const LogTemplateInvokeArgs *args, GString *result)
{
  UserTemplateFunction *self = (UserTemplateFunction *) s;

  log_template_append_format_with_context(self->template, args->messages, args->num_messages, args->opts, args->tz,
                                          args->seq_num, args->context_id, result);
}

static void
user_template_function_free(LogTemplateFunction *s)
{
  UserTemplateFunction *self = (UserTemplateFunction *) s;

  g_free(self->name);
  log_template_unref(self->template);
  g_free(self);
}

LogTemplateFunction *
user_template_function_new(const gchar *name, LogTemplate *template)
{
  UserTemplateFunction *self;

  self = g_new0(UserTemplateFunction, 1);
  self->super.size_of_state = 0;
  self->super.prepare = user_template_function_prepare;
  self->super.call = user_template_function_call;
  self->super.free_fn = user_template_function_free;
  self->template = log_template_ref(template);
  self->name = g_strdup(name);
  return &self->super;
}

typedef struct _UserTemplateFunctionPlugin
{
  Plugin super;
  LogTemplate *template;
} UserTemplateFunctionPlugin;

static gpointer
user_template_function_construct(Plugin *s)
{
  UserTemplateFunctionPlugin *self = (UserTemplateFunctionPlugin *) s;

  return user_template_function_new(self->super.name, self->template);
}

static void
user_template_function_plugin_free(Plugin *s)
{
  UserTemplateFunctionPlugin *plugin = (UserTemplateFunctionPlugin *) s;

  g_free((gchar *) plugin->super.name);
  log_template_unref(plugin->template);
  g_free(s);
}

void
user_template_function_register(GlobalConfig *cfg, const gchar *name, LogTemplate *template)
{
  UserTemplateFunctionPlugin *plugin;

  plugin = g_new0(UserTemplateFunctionPlugin, 1);
  plugin->super.type = LL_CONTEXT_TEMPLATE_FUNC;
  plugin->super.name = g_strdup(name);
  plugin->super.parser = NULL;
  plugin->super.construct = user_template_function_construct;
  plugin->super.free_fn = user_template_function_plugin_free;
  plugin->template = log_template_ref(template);
  plugin_register(&cfg->plugin_context, &plugin->super, 1);
}
