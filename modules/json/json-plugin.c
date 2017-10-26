/*
 * Copyright (c) 2011-2012 Balabit
 * Copyright (c) 2011-2012 Gergely Nagy <algernon@balabit.hu>
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
 */
#include "json-parser.h"
#include "format-json.h"
#include "json-parser-parser.h"
#include "plugin.h"
#include "plugin-types.h"

extern CfgParser json_parser_parser;

static Plugin json_plugins[] =
{
  {
    .type = LL_CONTEXT_PARSER,
    .name = "json-parser",
    .parser = &json_parser_parser,
  },
  TEMPLATE_FUNCTION_PLUGIN(tf_json, "format_json"),
};

gboolean
json_plugin_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, json_plugins, G_N_ELEMENTS(json_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "json-plugin",
  .version = SYSLOG_NG_VERSION,
  .description = "The json module provides JSON parsing & formatting support for syslog-ng.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = json_plugins,
  .plugins_len = G_N_ELEMENTS(json_plugins),
};
