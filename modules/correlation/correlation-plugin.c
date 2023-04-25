/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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
#include "dbparser.h"
#include "groupingby.h"
#include "plugin.h"
#include "plugin-types.h"

extern CfgParser correlation_parser;

static Plugin correlation_plugins[] =
{
  {
    .type = LL_CONTEXT_PARSER,
    .name = "db-parser",
    .parser = &correlation_parser,
  },
  {
    .type = LL_CONTEXT_PARSER,
    .name = "grouping-by",
    .parser = &correlation_parser,
  },
  {
    .type = LL_CONTEXT_PARSER,
    .name = "group-lines",
    .parser = &correlation_parser,
  },
};

gboolean
correlation_module_init(PluginContext *context, CfgArgs *args)
{
  pattern_db_global_init();
  grouping_parser_global_init();
  plugin_register(context, correlation_plugins, G_N_ELEMENTS(correlation_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "correlation",
  .version = SYSLOG_NG_VERSION,
  .description = "The correlation module implements db-parser(), grouping-by() and other correlation based functionality for syslog-ng.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = correlation_plugins,
  .plugins_len = G_N_ELEMENTS(correlation_plugins),
};
