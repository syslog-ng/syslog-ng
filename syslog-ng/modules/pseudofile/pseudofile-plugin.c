/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

extern CfgParser pseudofile_parser;

static Plugin pseudofile_plugins[] =
{
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "pseudofile",
    .parser = &pseudofile_parser,
  },
};

gboolean
pseudofile_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, pseudofile_plugins, G_N_ELEMENTS(pseudofile_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "pseudofile",
  .version = SYSLOG_NG_VERSION,
  .description = "The pseudofile module provides the pseudofile() destination for syslog-ng",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = pseudofile_plugins,
  .plugins_len = G_N_ELEMENTS(pseudofile_plugins),
};
