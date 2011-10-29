/*
 * Copyright (c) 2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2011 Gergely Nagy <algernon@balabit.hu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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
 */

#include "cfg-parser.h"
#include "plugin.h"
#include "jsonparser.h"

extern CfgParser jsonparser_parser;

static Plugin jsonparser_plugins[] =
{
  {
    .type = LL_CONTEXT_PARSER,
    .name = "json-parser",
    .parser = &jsonparser_parser,
  },
};

gboolean
jsonparser_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, jsonparser_plugins, G_N_ELEMENTS(jsonparser_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "jsonparser",
  .version = VERSION,
  .description = "The jsonparser module provides JSON parsing support for syslog-ng.",
  .core_revision = SOURCE_REVISION,
  .plugins = jsonparser_plugins,
  .plugins_len = G_N_ELEMENTS(jsonparser_plugins),
};
