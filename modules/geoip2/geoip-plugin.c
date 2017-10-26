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
#include "geoip-parser.h"
#include "tfgeoip.c"
#include "plugin.h"
#include "plugin-types.h"

extern CfgParser geoip2_parser_parser;

static Plugin geoip2_plugins[] =
{
  {
    .type = LL_CONTEXT_PARSER,
    .name = "geoip2",
    .parser = &geoip2_parser_parser,
  },
  TEMPLATE_FUNCTION_PLUGIN(tf_geoip_maxminddb, "geoip2"),
};

gboolean
geoip2_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, geoip2_plugins, G_N_ELEMENTS(geoip2_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "geoip2",
  .version = SYSLOG_NG_VERSION,
  .description = "The geoip2 module provides GeoIP2 (maxmind) support for syslog-ng.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = geoip2_plugins,
  .plugins_len = G_N_ELEMENTS(geoip2_plugins),
};
