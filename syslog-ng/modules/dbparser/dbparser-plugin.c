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

extern CfgParser dbparser_parser;

static Plugin dbparser_plugins[] =
{
  {
    .type = LL_CONTEXT_PARSER,
    .name = "db-parser",
    .parser = &dbparser_parser,
  },
  {
    .type = LL_CONTEXT_PARSER,
    .name = "grouping-by",
    .parser = &dbparser_parser,
  },
};

gboolean
dbparser_module_init(PluginContext *context, CfgArgs *args)
{
  pattern_db_global_init();
  grouping_by_global_init();
  plugin_register(context, dbparser_plugins, G_N_ELEMENTS(dbparser_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "dbparser",
  .version = SYSLOG_NG_VERSION,
  .description = "The db-parser() module implements sample database based parsing for syslog-ng.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = dbparser_plugins,
  .plugins_len = G_N_ELEMENTS(dbparser_plugins),
};
