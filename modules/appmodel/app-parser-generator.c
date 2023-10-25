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

#include "app-parser-generator.h"
#include "appmodel.h"

#include <string.h>

typedef struct _AppObjectGenerator AppObjectGenerator;

struct _AppObjectGenerator
{
  CfgBlockGenerator super;
  gboolean (*parse_arguments)(AppObjectGenerator *self, CfgArgs *args, const gchar *reference);
  void (*generate_config)(AppObjectGenerator *self, AppModelContext *appmodel, GString *result);
  const gchar *included_apps;
  const gchar *excluded_apps;
  gboolean is_parsing_enabled;
};

static gboolean
_is_application_included(AppObjectGenerator *self, const gchar *app_name)
{
  /* include everything if we don't have the option */
  if (!self->included_apps)
    return TRUE;
  return strstr(self->included_apps, app_name) != NULL;
}

static gboolean
_is_application_excluded(AppObjectGenerator *self, const gchar *app_name)
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


static gboolean
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
  AppModelContext *appmodel = appmodel_get_context(cfg);

  if (!self->parse_arguments(self, cfgargs, reference))
    return FALSE;

  self->generate_config(self, appmodel, result);

  return TRUE;
}

void
app_object_generator_init_instance(AppObjectGenerator *self, gint context, const gchar *name)
{
  cfg_block_generator_init_instance(&self->super, context, name);
  self->super.generate = _generate;
  self->parse_arguments = app_object_generator_parse_arguments_method;
}

/* app-parser() */

typedef struct _AppParserGenerator
{
  AppObjectGenerator super;
  const gchar *topic;
  GString *block;
} AppParserGenerator;

static gboolean
_parse_topic_arg(AppParserGenerator *self, CfgArgs *args, const gchar *reference)
{
  self->topic = cfg_args_get(args, "topic");
  if (!self->topic)
    {
      msg_error("app-parser() requires a topic() argument",
                evt_tag_str("reference", reference));
      return FALSE;
    }
  return TRUE;
}

static gboolean
app_parser_generator_parse_arguments(AppObjectGenerator *s, CfgArgs *args, const gchar *reference)
{
  AppParserGenerator *self = (AppParserGenerator *) s;
  g_assert(args != NULL);

  if (!_parse_topic_arg(self, args, reference))
    return FALSE;

  if (!app_object_generator_parse_arguments_method(&self->super, args, reference))
    return FALSE;

  return TRUE;
}

static void
_generate_filter(AppParserGenerator *self, const gchar *filter_expr)
{
  if (filter_expr)
    g_string_append_printf(self->block, "    filter { %s };\n", filter_expr);
}

static void
_generate_parser(AppParserGenerator *self, const gchar *parser_expr)
{
  if (parser_expr)
    g_string_append_printf(self->block, "    parser { %s };\n", parser_expr);
}

static void
_generate_action(AppParserGenerator *self, Application *app)
{
  g_string_append_printf(self->block,
                         "    rewrite {\n"
                         "       set-tag('.app.%s');\n"
                         "       set('%s' value('.app.name'));\n"
                         "    };\n"
                         "    flags(final);\n",
                         app->super.name, app->super.name);
}

static void
_generate_application(AppModelObject *object, gpointer user_data)
{
  Application *app = (Application *) object;
  AppParserGenerator *self = (AppParserGenerator *) user_data;

  if (strcmp(self->topic, object->instance) != 0)
    return;

  if (!_is_application_included(&self->super, object->name))
    return;

  if (_is_application_excluded(&self->super, object->name))
    return;

  g_string_append_printf(self->block, "\n#Start Application %s\n", object->name);
  g_string_append(self->block, "channel {\n");
  _generate_filter(self, app->filter_expr);
  _generate_parser(self, app->parser_expr);
  _generate_action(self, app);
  g_string_append(self->block, "};\n");
  g_string_append_printf(self->block, "\n#End Application %s\n", object->name);
}

static void
_generate_applications(AppParserGenerator *self, AppModelContext *appmodel)
{
  appmodel_context_iter_objects(appmodel, APPLICATION_TYPE_NAME, _generate_application, self);
}

static void
_generate_empty_frame(AppParserGenerator *self)
{
  g_string_append(self->block, "\nchannel { filter { tags('.app.doesnotexist'); }; flags(final); };");
}

static void
_generate_framing(AppParserGenerator *self, AppModelContext *appmodel)
{
  g_string_append(self->block,
                  "\nchannel {\n"
                  "    junction {\n");

  _generate_applications(self, appmodel);
  _generate_empty_frame(self);
  g_string_append(self->block, "    };\n");
  g_string_append(self->block, "}");
}

static void
app_parser_generate_config(AppObjectGenerator *s, AppModelContext *appmodel, GString *result)
{
  AppParserGenerator *self = (AppParserGenerator *) s;

  self->block = result;
  if (self->super.is_parsing_enabled)
    _generate_framing(self, appmodel);
  else
    _generate_empty_frame(self);
  self->block = NULL;
}


CfgBlockGenerator *
app_parser_generator_new(gint context, const gchar *name)
{
  AppParserGenerator *self = g_new0(AppParserGenerator, 1);

  app_object_generator_init_instance(&self->super, context, name);
  self->super.parse_arguments = app_parser_generator_parse_arguments;
  self->super.generate_config = app_parser_generate_config;
  return &self->super.super;
}

/* app-transform() */

typedef struct _AppTransformGenerator
{
  AppObjectGenerator super;
  const gchar *target;
  GString *block;
} AppTransformGenerator;

static gboolean
_parse_target_arg(AppTransformGenerator *self, CfgArgs *args, const gchar *reference)
{
  self->target = cfg_args_get(args, "target");
  if (!self->target)
    {
      msg_error("app-transform() requires a target() argument",
                evt_tag_str("reference", reference));
      return FALSE;
    }
  return TRUE;
}

static gboolean
app_transform_generator_parse_arguments(AppObjectGenerator *s, CfgArgs *args, const gchar *reference)
{
  AppTransformGenerator *self = (AppTransformGenerator *) s;
  g_assert(args != NULL);

  if (!_parse_target_arg(self, args, reference))
    return FALSE;

  if (!app_object_generator_parse_arguments_method(&self->super, args, reference))
    return FALSE;

  return TRUE;
}

static void
_generate_app_transform(AppModelObject *object, gpointer user_data)
{
  Transformation *transformation = (Transformation *) object;
  AppTransformGenerator *self = (AppTransformGenerator *) user_data;

  if (strcmp(self->target, object->instance) != 0)
    return;

  if (!_is_application_included(&self->super, object->name))
    return;

  if (_is_application_excluded(&self->super, object->name))
    return;

  g_string_append_printf(self->block, "\n#Start Application %s\n", object->name);
  g_string_append(self->block, "channel {\n");

  g_string_append_printf(self->block, "    filter { tags(\".app.%s\"); };\n", object->name);
  g_string_append_printf(self->block, "    %s\n", transformation->translate_expr);
  g_string_append(self->block, "    flags(final);\n");
  g_string_append(self->block, "};\n");
  g_string_append_printf(self->block, "\n#End Application %s\n", object->name);
}


static void
app_transform_generate_config(AppObjectGenerator *s, AppModelContext *appmodel, GString *result)
{
  AppTransformGenerator *self = (AppTransformGenerator *) s;

  self->block = result;
  g_string_append_printf(result, "## app-transform(target(%s))\nchannel {", self->target);
  appmodel_context_iter_objects(appmodel, TRANSFORMATION_TYPE_NAME, _generate_app_transform, self);
  g_string_append(result, "}");
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
