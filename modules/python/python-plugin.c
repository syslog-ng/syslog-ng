/*
 * Copyright (c) 2014-2015 Balabit
 * Copyright (c) 2014 Gergely Nagy <algernon@balabit.hu>
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
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

#include "python-module.h"
#include "python-tf.h"
#include "python-startup.h"
#include "python-debugger.h"
#include "plugin.h"
#include "plugin-types.h"

#include <stdlib.h>
#include <string.h>

extern CfgParser python_parser;
extern CfgParser python_parser_parser;

static Plugin python_plugins[] =
{
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "python",
    .parser = &python_parser,
  },
  {
    .type = LL_CONTEXT_INNER_DEST,
    .name = "python_http_header",
    .parser = &python_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "python",
    .parser = &python_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "python_fetcher",
    .parser = &python_parser,
  },
  {
    .type = LL_CONTEXT_ROOT,
    .name = "python",
    .parser = &python_parser,
  },
  {
    .type = LL_CONTEXT_PARSER,
    .name = "python",
    .parser = &python_parser,
  },
  TEMPLATE_FUNCTION_PLUGIN(tf_python, "python"),
};

gboolean
python_module_init(PluginContext *context, CfgArgs *args)
{
  gboolean use_virtualenv = args ? cfg_args_get_as_boolean(args, "use-virtualenv", TRUE) : TRUE;
  if (!_py_init_interpreter(use_virtualenv))
    return FALSE;
  python_debugger_init();
  plugin_register(context, python_plugins, G_N_ELEMENTS(python_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "python",
  .version = SYSLOG_NG_VERSION,
  .description = "The python ("PYTHON_MODULE_VERSION") module provides Python scripting support for syslog-ng.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = python_plugins,
  .plugins_len = G_N_ELEMENTS(python_plugins),
};
