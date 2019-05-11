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

typedef struct _AppParserGenerator
{
  CfgBlockGenerator super;
  GString *block;
  const gchar *topic;
  const gchar *included_apps;
  const gchar *excluded_apps;
  gboolean is_parsing_enabled;
} AppParserGenerator;

static const gchar *
_get_filter_expr(Application *app, Application *base_app)
{
  if (app->filter_expr)
    return app->filter_expr;
  if (base_app)
    return base_app->filter_expr;
  return NULL;
}

static const gchar *
_get_parser_expr(Application *app, Application *base_app)
{
  if (app->parser_expr)
    return app->parser_expr;
  if (base_app)
    return base_app->parser_expr;
  return NULL;
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
                         app->name, app->name);
}

static gboolean
_is_application_included(AppParserGenerator *self, Application *app)
{
  /* include everything if we don't have the option */
  if (!self->included_apps)
    return TRUE;
  return strstr(self->included_apps, app->name) != NULL;
}

static gboolean
_is_application_excluded(AppParserGenerator *self, Application *app)
{
  if (!self->excluded_apps)
    return FALSE;
  return strstr(self->excluded_apps, app->name) != NULL;
}

static void
_generate_application(Application *app, Application *base_app, gpointer user_data)
{
  AppParserGenerator *self = (AppParserGenerator *) user_data;

  if (strcmp(self->topic, app->topic) != 0)
    return;

  if (!_is_application_included(self, app))
    return;

  if (_is_application_excluded(self, app))
    return;

  g_string_append_printf(self->block, "\n#Start Application %s\n",app->name);
  g_string_append(self->block, "channel {\n");
  _generate_filter(self, _get_filter_expr(app, base_app));
  _generate_parser(self, _get_parser_expr(app, base_app));
  _generate_action(self, app);
  g_string_append(self->block, "};\n");
  g_string_append_printf(self->block, "\n#End Application %s\n",app->name);

}

static void
_generate_applications(AppParserGenerator *self, AppModelContext *appmodel)
{
  appmodel_context_iter_applications(appmodel, _generate_application, self);
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


static gboolean
_parse_auto_parse_arg(AppParserGenerator *self, CfgArgs *args, const gchar *reference)
{
  const gchar *v = cfg_args_get(args, "auto-parse");

  if (v)
    self->is_parsing_enabled = cfg_process_yesno(v);
  else
    self->is_parsing_enabled = TRUE;
  return TRUE;
}

static gboolean
_parse_auto_parse_exclude_arg(AppParserGenerator *self, CfgArgs *args, const gchar *reference)
{
  const gchar *v = cfg_args_get(args, "auto-parse-exclude");
  if (!v)
    return TRUE;
  self->excluded_apps = g_strdup(v);
  return TRUE;
}

static gboolean
_parse_auto_parse_include_arg(AppParserGenerator *self, CfgArgs *args, const gchar *reference)
{
  const gchar *v = cfg_args_get(args, "auto-parse-include");
  if (!v)
    return TRUE;
  self->included_apps = g_strdup(v);
  return TRUE;
}

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
_parse_arguments(AppParserGenerator *self, CfgArgs *args, const gchar *reference)
{
  g_assert(args != NULL);

  if (!_parse_topic_arg(self, args, reference))
    return FALSE;
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
  AppParserGenerator *self = (AppParserGenerator *) s;
  AppModelContext *appmodel = appmodel_get_context(cfg);
  CfgArgs *cfgargs = (CfgArgs *)args;

  if (!_parse_arguments(self, cfgargs, reference))
    return FALSE;

  self->block = result;
  if (self->is_parsing_enabled)
    _generate_framing(self, appmodel);
  else
    _generate_empty_frame(self);
  self->block = NULL;

  return TRUE;
}

CfgBlockGenerator *
app_parser_generator_new(gint context, const gchar *name)
{
  AppParserGenerator *self = g_new0(AppParserGenerator, 1);

  cfg_block_generator_init_instance(&self->super, context, name);
  self->super.generate = _generate;
  return &self->super;
}
