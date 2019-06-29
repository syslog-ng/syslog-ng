/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Vincent Bernat <Vincent.Bernat@exoscale.ch>
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

#include "timestamp-parser.h"

#include "plugin.h"
#include "plugin-types.h"

extern CfgParser timestamp_parser;

static Plugin timestamp_plugins[] =
{
  {
    .type = LL_CONTEXT_PARSER,
    .name = "date-parser",
    .parser = &timestamp_parser,
  },
  {
    .type = LL_CONTEXT_REWRITE,
    .name = "fix-time-zone",
    .parser = &timestamp_parser,
  },
};

gboolean
timestamp_module_init(PluginContext *context, CfgArgs *args G_GNUC_UNUSED)
{
  plugin_register(context, timestamp_plugins, G_N_ELEMENTS(timestamp_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "timestamp",
  .version = SYSLOG_NG_VERSION,
  .description = "The timestamp module provides support for manipulating timestamps in syslog-ng.",
  .core_revision = VERSION_CURRENT_VER_ONLY,
  .plugins = timestamp_plugins,
  .plugins_len = G_N_ELEMENTS(timestamp_plugins),
};
