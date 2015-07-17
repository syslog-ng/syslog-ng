/*
 * Copyright (c) 2015 BalaBit
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

#include "plugin.h"
#include "plugin-types.h"
#include "format-welf.h"

extern CfgParser welf_parser_parser;

static Plugin welf_plugins[] =
{
/*  {
    .type = LL_CONTEXT_PARSER,
    .name = "welf",
    .parser = &welf_parser_parser,
  }, */
  TEMPLATE_FUNCTION_PLUGIN(tf_format_welf, "format-welf"),
};

gboolean
welf_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, welf_plugins, G_N_ELEMENTS(welf_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "welf",
  .version = VERSION,
  .description = "The welf module provides WebTrends Enhanced Log Format support for syslog-ng.",
  .core_revision = SOURCE_REVISION,
  .plugins = welf_plugins,
  .plugins_len = G_N_ELEMENTS(welf_plugins),
};
