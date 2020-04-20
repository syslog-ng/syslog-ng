/*
 * Copyright (c) 2016 Balabit
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
#include "add-contextual-data.h"
#include "plugin.h"
#include "plugin-types.h"

extern CfgParser add_contextual_data_parser;

static Plugin add_contextual_data_plugins[] =
{
  {
    .type = LL_CONTEXT_PARSER,
    .name = "add_contextual_data",
    .parser = &add_contextual_data_parser,
  },
};

gboolean
add_contextual_data_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, add_contextual_data_plugins,
                  G_N_ELEMENTS(add_contextual_data_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "add_contextual_data",
  .version = SYSLOG_NG_VERSION,
  .description =
  "The add_contextual_data module provides parsing support for CSV and other separated value formats for syslog-ng.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = add_contextual_data_plugins,
  .plugins_len = G_N_ELEMENTS(add_contextual_data_plugins),
};
