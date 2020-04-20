/*
 * Copyright (c) 2002-2017 Balabit
 * Copyright (c) 1998-2017 Balázs Scheidler
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
#include "tags-parser.h"
#include "plugin.h"
#include "plugin-types.h"

extern CfgParser tags_parser_parser;

static Plugin tags_parser_plugins[] =
{
  {
    .type = LL_CONTEXT_PARSER,
    .name = "tags-parser",
    .parser = &tags_parser_parser,
  },
};

gboolean
tags_parser_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, tags_parser_plugins, G_N_ELEMENTS(tags_parser_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "tags_parser",
  .version = SYSLOG_NG_VERSION,
  .description = "The tags_parser module provides parsing support for restoring message tags as produced by the TAGS macro.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = tags_parser_plugins,
  .plugins_len = G_N_ELEMENTS(tags_parser_plugins),
};
