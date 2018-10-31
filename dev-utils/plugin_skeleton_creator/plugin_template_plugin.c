/*
 * Copyright (c) @YEAR_AND_AUTHOR@
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
#include "plugin.h"
#include "plugin-types.h"

extern CfgParser @PLUGIN_NAME_US@_parser;

static Plugin @PLUGIN_NAME_US@_plugins[] =
{
  
    .type = @PLUGIN_TYPE@,
    .name = "@PLUGIN_KEY@",
    .parser = &@PLUGIN_NAME_US@_parser,
  
};
gboolean
@PLUGIN_NAME_US@_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, @PLUGIN_NAME_US@_plugins, G_N_ELEMENTS(@PLUGIN_NAME_US@_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "@PLUGIN_NAME_US@",
  .version = SYSLOG_NG_VERSION,
  .description = "Please fill this description",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = @PLUGIN_NAME_US@_plugins,
  .plugins_len = G_N_ELEMENTS(@PLUGIN_NAME_US@_plugins),
};
