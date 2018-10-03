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
#include "python-parser.h"
#include "python-dest.h"
#include "python-tf.h"
#include "python-logmsg.h"
#include "python-logtemplate.h"
#include "python-integerpointer.h"
#include "python-source.h"
#include "python-global-code-loader.h"
#include "python-debugger.h"

#include "plugin.h"
#include "plugin-types.h"

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
    .type = LL_CONTEXT_SOURCE,
    .name = "python",
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
static gboolean interpreter_initialized = FALSE;

static void
_py_init_interpreter(void)
{
  if (!interpreter_initialized)
    {
      python_debugger_append_inittab();

      Py_Initialize();
      py_init_argv();

      PyEval_InitThreads();
      py_datetime_init();
      py_log_message_init();
      py_log_template_init();
      py_integer_pointer_init();
      py_log_source_init();
      py_global_code_loader_init();
      PyEval_SaveThread();

      interpreter_initialized = TRUE;
    }
}

gboolean
python_module_init(PluginContext *context, CfgArgs *args G_GNUC_UNUSED)
{
  _py_init_interpreter();
  python_debugger_init();
  plugin_register(context, python_plugins, G_N_ELEMENTS(python_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "python",
  .version = SYSLOG_NG_VERSION,
  .description = "The python ("PYTHON_MODULE_VERSION") module provides Python scripted destination support for syslog-ng.",
  .core_revision = VERSION_CURRENT_VER_ONLY,
  .plugins = python_plugins,
  .plugins_len = G_N_ELEMENTS(python_plugins),
};
