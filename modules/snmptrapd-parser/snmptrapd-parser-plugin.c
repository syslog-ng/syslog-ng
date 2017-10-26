/*
 * Copyright (c) 2017 Balabit
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

extern CfgParser snmptrapd_parser_parser;

static Plugin snmptrapd_parser_plugins[] =
{
  {
    .type = LL_CONTEXT_PARSER,
    .name = "snmptrapd-parser",
    .parser = &snmptrapd_parser_parser,
  },
};

gboolean
snmptrapd_parser_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, snmptrapd_parser_plugins, G_N_ELEMENTS(snmptrapd_parser_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "snmptrapd_parser",
  .version = SYSLOG_NG_VERSION,
  .description = "The snmptrapd module provides parsing support for snmptrapd output in syslog-ng",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = snmptrapd_parser_plugins,
  .plugins_len = G_N_ELEMENTS(snmptrapd_parser_plugins),
};

