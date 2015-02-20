/*
 * Copyright (c) 2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2014 Gergely Nagy <algernon@balabit.hu>
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
#include "python-parser.h"
#include "python-dest.h"
#include "python-logmsg.h"

#include "plugin.h"
#include "plugin-types.h"

extern CfgParser python_parser;

static Plugin python_plugins[] =
{
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "python",
    .parser = &python_parser,
  },
};
static gboolean interpreter_initialized = FALSE;

static void
_py_init_interpreter(void)
{
  if (!interpreter_initialized)
    {
      Py_Initialize();

      PyEval_InitThreads();
      python_log_message_init();
      PyEval_ReleaseLock();

      interpreter_initialized = TRUE;
    }
}

gboolean
python_module_init(GlobalConfig *cfg, CfgArgs *args G_GNUC_UNUSED)
{
  _py_init_interpreter();
  plugin_register(cfg, python_plugins, G_N_ELEMENTS(python_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "python",
  .version = VERSION,
  .description = "The python module provides Python scripted destination support for syslog-ng.",
  .core_revision = VERSION_CURRENT_VER_ONLY,
  .plugins = python_plugins,
  .plugins_len = G_N_ELEMENTS(python_plugins),
};
