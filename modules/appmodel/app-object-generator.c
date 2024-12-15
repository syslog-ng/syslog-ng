/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balazs Scheidler <balazs.scheidler@balabit.com>
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

#include "app-object-generator.h"
#include "appmodel.h"

#include <string.h>

gboolean
app_object_generator_is_application_included(AppObjectGenerator *self, const gchar *app_name)
{
  /* include everything if we don't have the option */
  if (!self->included_apps)
    return TRUE;
  return strstr(self->included_apps, app_name) != NULL;
}

gboolean
app_object_generator_is_application_excluded(AppObjectGenerator *self, const gchar *app_name)
{
  if (!self->excluded_apps)
    return FALSE;
  return strstr(self->excluded_apps, app_name) != NULL;
}

static gboolean
_parse_auto_parse_arg(AppObjectGenerator *self, CfgArgs *args, const gchar *reference)
{
  const gchar *v = cfg_args_get(args, "auto-parse");

  if (v)
    self->is_parsing_enabled = cfg_process_yesno(v);
  else
    self->is_parsing_enabled = TRUE;
  return TRUE;
}

static gboolean
_parse_auto_parse_exclude_arg(AppObjectGenerator *self, CfgArgs *args, const gchar *reference)
{
  const gchar *v = cfg_args_get(args, "auto-parse-exclude");
  if (!v)
    return TRUE;
  self->excluded_apps = g_strdup(v);
  return TRUE;
}

static gboolean
_parse_auto_parse_include_arg(AppObjectGenerator *self, CfgArgs *args, const gchar *reference)
{
  const gchar *v = cfg_args_get(args, "auto-parse-include");
  if (!v)
    return TRUE;
  self->included_apps = g_strdup(v);
  return TRUE;
}

gboolean
app_object_generator_parse_arguments_method(AppObjectGenerator *self, CfgArgs *args, const gchar *reference)
{
  g_assert(args != NULL);

  if (!_parse_auto_parse_arg(self, args, reference))
    return FALSE;
  if (!_parse_auto_parse_exclude_arg(self, args, reference))
    return FALSE;
  if (!_parse_auto_parse_include_arg(self, args, reference))
    return FALSE;
  return TRUE;
}

static gboolean
_generate(CfgBlockGenerator *s, GlobalConfig *cfg, gpointer args, GString *result, const gchar *reference)
{
  AppObjectGenerator *self = (AppObjectGenerator *) s;
  CfgArgs *cfgargs = (CfgArgs *)args;

  if (!self->parse_arguments(self, cfgargs, reference))
    return FALSE;

  self->generate_config(self, cfg, result);

  return TRUE;
}

void
app_object_generator_init_instance(AppObjectGenerator *self, gint context, const gchar *name)
{
  cfg_block_generator_init_instance(&self->super, context, name);
  self->super.generate = _generate;
  self->parse_arguments = app_object_generator_parse_arguments_method;
}
