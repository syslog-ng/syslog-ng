/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "afsocket-parser.h"
#include "cfg-parser.h"
#include "plugin.h"
#include "tlscontext.h"
#include "plugin-types.h"

static Plugin afsocket_plugins[] =
{
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "unix-stream",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "unix-stream",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "unix-dgram",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "unix-dgram",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "tcp",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "tcp",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "tcp6",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "tcp6",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "udp",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "udp",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "udp6",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "udp6",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "syslog",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "syslog",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "network",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "network",
    .parser = &afsocket_parser,
  },
};

gboolean
afsocket_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, afsocket_plugins, G_N_ELEMENTS(afsocket_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "afsocket",
  .version = VERSION,
#if BUILD_WITH_SSL
  .preference = 100,
  .description = "The afsocket module provides socket based transports for syslog-ng, such as the udp(), tcp() and syslog() drivers. This module is compiled with SSL support.",
#else
  .description = "The afsocket module provides socket based transports for syslog-ng, such as the udp(), tcp() and syslog() drivers. This module is compiled without SSL support.",
#endif
  .core_revision = SOURCE_REVISION,
  .plugins = afsocket_plugins,
  .plugins_len = G_N_ELEMENTS(afsocket_plugins),
};
