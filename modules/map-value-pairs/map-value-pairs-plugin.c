/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balazs Scheidler <balazs.scheidler@balabit.com>
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

extern CfgParser map_value_pairs_parser;

static Plugin map_value_pairs_plugins[] =
{
  {
    .type = LL_CONTEXT_PARSER,
    .name = "map_value_pairs",
    .parser = &map_value_pairs_parser,
  },
};

gboolean
map_value_pairs_module_init(PluginContext *context, CfgArgs *args G_GNUC_UNUSED)
{
  plugin_register(context, map_value_pairs_plugins, G_N_ELEMENTS(map_value_pairs_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "map-value-pairs",
  .version = SYSLOG_NG_VERSION,
  .description = "The map-names module provides the map-names() parser for syslog-ng.",
  .core_revision = VERSION_CURRENT_VER_ONLY,
  .plugins = map_value_pairs_plugins,
  .plugins_len = G_N_ELEMENTS(map_value_pairs_plugins),
};
