/*
 * Copyright (c) 2022 One Identity
 * Copyright (c) 2022 Kokan
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
#include "flip-parser.h"
#include "plugin.h"
#include "plugin-types.h"

extern CfgParser flip_parser_parser;

static Plugin flip_parser_plugins[] =
{
  {
    .type = LL_CONTEXT_PARSER,
    .name = "flip-parser",
    .parser = &flip_parser_parser,
  },
};

gboolean
flip_parser_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, flip_parser_plugins, G_N_ELEMENTS(flip_parser_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "flip-parser",
  .version = SYSLOG_NG_VERSION,
  .description = "The flip module provides flip parser that can flip or reverse the selected text.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = flip_parser_plugins,
  .plugins_len = G_N_ELEMENTS(flip_parser_plugins),
};
