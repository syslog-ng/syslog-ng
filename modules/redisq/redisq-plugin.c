/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 Mehul Prajapati <mehulprajapati2802@gmail.com>
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

extern CfgParser redisq_parser;

static Plugin redisq_plugins[] =
{
  {
    .type = LL_CONTEXT_INNER_DEST,
    .name = "redis_queue",
    .parser = &redisq_parser,
  },
};

#ifndef STATIC
const ModuleInfo module_info =
{
  .canonical_name = "redis_queue",
  .version = SYSLOG_NG_VERSION,
  .description = "This module provides redis protocol based queuing mechanism",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = redisq_plugins,
  .plugins_len = G_N_ELEMENTS(redisq_plugins),
};
#endif

gboolean
redis_queue_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, redisq_plugins, G_N_ELEMENTS(redisq_plugins));
  return TRUE;
}
