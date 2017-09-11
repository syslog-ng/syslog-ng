/*
 * Copyright (c) 2002-2015 Balabit
 * Copyright (c) 1998-2015 Balázs Scheidler
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

#include "cfg-parser.h"
#include "appmodel-parser.h"
#include "app-parser-generator.h"
#include "plugin.h"
#include "plugin-types.h"

extern CfgParser appmodel_parser;

static gpointer
app_parser_construct(Plugin *p)
{
  return app_parser_generator_new(p->type, p->name);
}

static Plugin appmodel_plugins[] =
{
  {
    .type = LL_CONTEXT_ROOT,
    .name = "application",
    .parser = &appmodel_parser,
  },
  {
    .type = LL_CONTEXT_PARSER | LL_CONTEXT_FLAG_GENERATOR,
    .name = "app-parser",
    .construct = app_parser_construct
  }
};

gboolean
appmodel_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, appmodel_plugins, G_N_ELEMENTS(appmodel_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "appmodel",
  .version = SYSLOG_NG_VERSION,
  .description = "The appmodel module provides parsing support for restoring message tags as produced by the TAGS macro.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = appmodel_plugins,
  .plugins_len = G_N_ELEMENTS(appmodel_plugins),
};
