/*
 * Copyright (c) 2023 László Várady
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
#include "protos/apphook.h"

extern CfgParser bigquery_parser;

static Plugin bigquery_plugins[] =
{
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "bigquery",
    .parser = &bigquery_parser,
  },
};

gboolean
bigquery_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, bigquery_plugins, G_N_ELEMENTS(bigquery_plugins));
  grpc_register_global_initializers();
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "bigquery",
  .version = SYSLOG_NG_VERSION,
  .description = "Google Cloud BigQuery plugins",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = bigquery_plugins,
  .plugins_len = G_N_ELEMENTS(bigquery_plugins),
};
