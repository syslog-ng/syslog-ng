/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Viktor Juhasz <viktor.juhasz@balabit.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <jni.h>

#include "java-parser.h"
#include "plugin.h"
#include "messages.h"
#include "stats/stats.h"
#include "logqueue.h"
#include "driver.h"
#include "plugin-types.h"


extern CfgParser java_parser;

static Plugin java_plugins[] =
{
  {
    .type = LL_CONTEXT_OPTIONS,
    .name = "jvm_options",
    .parser = &java_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "java",
    .parser = &java_parser,
  },
};

gboolean
java_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, java_plugins, G_N_ELEMENTS(java_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "java",
  .version = SYSLOG_NG_VERSION,
  .description = "The java module provides Java destination support for syslog-ng.",
  .core_revision = "Dummy Revision",
  .plugins = java_plugins,
  .plugins_len = G_N_ELEMENTS(java_plugins),
};
