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

/* app-parser() */

typedef struct _AppParserGenerator
{
  AppObjectGenerator super;
  const gchar *topic;
  GString *block;
  gboolean first_app_generated;
  gboolean allow_overlaps;
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
_parse_allow_overlaps(AppParserGenerator *self, CfgArgs *args, const gchar *reference)
{
  const gchar *v = cfg_args_get(args, "allow-overlaps");
  if (v)
    self->allow_overlaps = cfg_process_yesno(v);
  else
    self->allow_overlaps = FALSE;
  return TRUE;
}

static gboolean
app_parser_generator_parse_arguments(AppObjectGenerator *s, CfgArgs *args, const gchar *reference)
{
  AppParserGenerator *self = (AppParserGenerator *) s;
  g_assert(args != NULL);

  if (!_parse_topic_arg(self, args, reference))
    return FALSE;

  if (!_parse_allow_overlaps(self, args, reference))
    return FALSE;

  if (!app_object_generator_parse_arguments_method(&self->super, args, reference))
    return FALSE;

  return TRUE;
}

static void
_generate_filter(AppParserGenerator *self, const gchar *filter_expr)
{
  if (filter_expr)
    g_string_append_printf(self->block,
                           "            filter {\n"
                           "                %s\n"
                           "            };\n", filter_expr);
}

static void
_generate_parser(AppParserGenerator *self, const gchar *parser_expr)
{
  if (parser_expr)
    g_string_append_printf(self->block,
                           "            parser {\n"
                           "                %s\n"
                           "            };\n", parser_expr);
}

static void
_generate_filterx(AppParserGenerator *self, const gchar *filterx_expr)
{
  if (filterx_expr)
    g_string_append_printf(self->block,
                           "            filterx {\n"
                           "                %s\n"
                           "            };\n", filterx_expr);
}

static void
_generate_action(AppParserGenerator *self, Application *app)
{
  if (self->allow_overlaps)
    return;

  if (app->filterx_expr)
    g_string_append_printf(self->block,
                             "            filterx {\n"
                             "                meta.app_name = '%s';\n"
                             "            };\n",
                             app->super.name);

  else
    g_string_append_printf(self->block,
                             "            rewrite {\n"
                             "                set-tag('.app.%s');\n"
                             "                set('%s' value('.app.name'));\n"
                             "            };\n",
                             app->super.name, app->super.name);
}

static void
_generate_application(Application *app, gpointer user_data)
{
  AppParserGenerator *self = (AppParserGenerator *) user_data;

  if (strcmp(self->topic, app->super.instance) != 0)
    return;

  if (!app_object_generator_is_application_included(&self->super, app->super.name))
    return;

  if (app_object_generator_is_application_excluded(&self->super, app->super.name))
    return;

  if (self->first_app_generated)
    {
      if (self->allow_overlaps)
        g_string_append(self->block,
                        "        ;\n"
                        "        if {\n");
      else
        g_string_append(self->block,
                        "        elif {\n");
    }
  else
    {
      self->first_app_generated = TRUE;
      g_string_append(self->block,
                      "        if {\n");
    }
  g_string_append_printf(self->block,
                         "            #Start Application %s\n", app->super.name);

  _generate_filter(self, app->filter_expr);
  _generate_parser(self, app->parser_expr);
  _generate_filterx(self, app->filterx_expr);
  _generate_action(self, app);
  g_string_append_printf(self->block,
                         "            #End Application %s\n", app->super.name);
  g_string_append(self->block, "        }\n");
}

static void
_generate_applications(AppParserGenerator *self, GlobalConfig *cfg)
{
  appmodel_iter_applications(cfg, _generate_application, self);
}

static void
_generate_framing(AppParserGenerator *self, GlobalConfig *cfg)
{
  g_string_append(self->block,
                  "\nchannel {\n");

  self->first_app_generated = FALSE;
  if (!self->allow_overlaps)
    {
      _generate_applications(self, cfg);
      if (self->first_app_generated)
        g_string_append(self->block, "        else {\n");
      else
        g_string_append(self->block, "        channel {\n");

      g_string_append(self->block,
                      "            filterx { false; };\n"
                      "        };\n");
    }
  else
    {
      _generate_applications(self, cfg);
      if (self->first_app_generated)
        g_string_append(self->block, "        ;\n");
    }
  g_string_append(self->block, "}");
}

static void
_generate_empty_frame(AppParserGenerator *self)
{
  g_string_append(self->block, "channel { filterx { false; }; };");
}

void
app_parser_generate_config(AppObjectGenerator *s, GlobalConfig *cfg, GString *result)
{
  AppParserGenerator *self = (AppParserGenerator *) s;

  self->block = result;
  if (self->super.is_parsing_enabled)
    _generate_framing(self, cfg);
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
