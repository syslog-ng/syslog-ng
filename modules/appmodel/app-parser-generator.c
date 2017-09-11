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
  g_string_append_printf(self->block, "    rewrite { set-tag('.app.%s'); };\n", app->name);
  g_string_append(self->block, "    flags(final);\n");
}

static void
_generate_application(Application *app, Application *base_app, gpointer user_data)
{
  AppParserGenerator *self = (AppParserGenerator *) user_data;

  if (strcmp(self->topic, app->topic) != 0)
    return;

  g_string_append(self->block, "channel {\n");
  _generate_filter(self, _get_filter_expr(app, base_app));
  _generate_parser(self, _get_parser_expr(app, base_app));
  _generate_action(self, app);
  g_string_append(self->block, "};\n");

}

static void
_generate_applications(AppParserGenerator *self, GlobalConfig *cfg)
{
  AppModelContext *appmodel = appmodel_get_context(cfg);

  appmodel_context_iter_applications(appmodel, _generate_application, self);
}

static gboolean
_generate(CfgBlockGenerator *s, GlobalConfig *cfg, CfgArgs *args, GString *result)
{
  AppParserGenerator *self = (AppParserGenerator *) s;

  g_assert(args != NULL);
  self->topic = cfg_args_get(args, "topic");
  if (!self->topic)
    {
      msg_error("app-parser() requires a topic() argument");
      return FALSE;
    }

  self->block = result;
  g_string_append(self->block,
                  "\nchannel {\n"
                  "    junction {\n");

  _generate_applications(self, cfg);

  g_string_append(self->block, "    };\n");
  g_string_append(self->block, "}");
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
