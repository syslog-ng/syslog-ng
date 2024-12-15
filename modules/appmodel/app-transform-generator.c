/*
 * Copyright (c) 2024 Balazs Scheidler <balazs.scheidler@axoflow.com>
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

/* app-transform() */

typedef struct _AppTransformGenerator
{
  AppObjectGenerator super;
  const gchar *flavour;
  GString *block;
} AppTransformGenerator;

static gboolean
_parse_transforms_arg(AppTransformGenerator *self, CfgArgs *args, const gchar *reference)
{
  self->flavour = cfg_args_get(args, "flavour");
  if (!self->flavour)
    self->flavour = "default";
  return TRUE;
}

static gboolean
app_transform_generator_parse_arguments(AppObjectGenerator *s, CfgArgs *args, const gchar *reference)
{
  AppTransformGenerator *self = (AppTransformGenerator *) s;
  g_assert(args != NULL);

  if (!_parse_transforms_arg(self, args, reference))
    return FALSE;

  if (!app_object_generator_parse_arguments_method(&self->super, args, reference))
    return FALSE;

  return TRUE;
}

static void
_generate_steps(AppTransformGenerator *self, GList *steps)
{
  for (GList *l = steps; l; l = l->next)
    {
      TransformationStep *step = l->data;
      g_string_append_printf(self->block, "        # step: %s", step->name);
      g_string_append_printf(self->block, "        %s\n", step->expr);
    }
}

static void
_generate_app_transform(Transformation *transformation, gpointer user_data)
{
  AppTransformGenerator *self = (AppTransformGenerator *) user_data;

  if (strcmp(transformation->super.instance, self->flavour) != 0)
    return;

  if (!app_object_generator_is_application_included(&self->super, transformation->super.name))
    return;

  if (app_object_generator_is_application_excluded(&self->super, transformation->super.name))
    return;

  g_string_append_printf(self->block, "\n#Start Application %s\n", transformation->super.name);
  g_string_append_printf(self->block,     "    if (meta.app_name == '%s') {\n", transformation->super.name);
  for (GList *l = transformation->blocks; l; l = l->next)
    {
      TransformationBlock *block = l->data;

      _generate_steps(self, block->steps);
    }
  g_string_append(self->block,            "    };\n");
  g_string_append_printf(self->block, "\n#End Application %s\n", transformation->super.name);
}


static void
app_transform_generate_config(AppObjectGenerator *s, GlobalConfig *cfg, GString *result)
{
  AppTransformGenerator *self = (AppTransformGenerator *) s;

  self->block = result;
  g_string_append_printf(result, "## app-transform(flavour(%s))\n"
                                 "channel {\n"
                                 "    filterx {\n", self->flavour);
  appmodel_iter_transformations(cfg, _generate_app_transform, self);
  g_string_append(result,        "    };\n"
                                 "}");
  self->block = NULL;
}

CfgBlockGenerator *
app_transform_generator_new(gint context, const gchar *name)
{
  AppTransformGenerator *self = g_new0(AppTransformGenerator, 1);

  app_object_generator_init_instance(&self->super, context, name);
  self->super.parse_arguments = app_transform_generator_parse_arguments;
  self->super.generate_config = app_transform_generate_config;
  return &self->super.super;
}
