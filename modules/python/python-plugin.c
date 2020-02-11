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
#include "syslog-ng.h"
#include "python-parser.h"
#include "python-dest.h"
#include "python-tf.h"
#include "python-logmsg.h"
#include "python-logtemplate.h"
#include "python-integerpointer.h"
#include "python-logger.h"
#include "python-source.h"
#include "python-fetcher.h"
#include "python-global-code-loader.h"
#include "python-debugger.h"

#include "plugin.h"
#include "plugin-types.h"
#include "reloc.h"

#include <stdlib.h>
#include <string.h>

extern CfgParser python_parser;
extern CfgParser python_parser_parser;

#define PYTHON_PLUGIN(_name, _type) \
  { \
    .type = _type, \
    .name = _name, \
    .parser = &python_parser, \
  }

#define PYTHON_PLUGINS(version) \
  PYTHON_PLUGIN("python" #version, LL_CONTEXT_DESTINATION), \
  PYTHON_PLUGIN("python" #version, LL_CONTEXT_SOURCE), \
  PYTHON_PLUGIN("python" #version, LL_CONTEXT_ROOT), \
  PYTHON_PLUGIN("python" #version, LL_CONTEXT_PARSER), \
  PYTHON_PLUGIN("python" #version "_fetcher", LL_CONTEXT_SOURCE), \
  TEMPLATE_FUNCTION_PLUGIN(tf_python, "python" #version)


static Plugin python_plugins[] =
{
#if SYSLOG_NG_ENABLE_PYTHONv2
  PYTHON_PLUGINS(),
  PYTHON_PLUGINS(2),
#elif SYSLOG_NG_ENABLE_PYTHONv3
  PYTHON_PLUGINS(3),
#endif

#if SYSLOG_NG_ENABLE_PYTHONv3 && !SYSLOG_NG_ENABLE_PYTHONv2_AND_v3
  /* python() keyword compat for Python 3 */
  PYTHON_PLUGINS(),
#endif
};

static gboolean interpreter_initialized = FALSE;

static gboolean
_check_conflicting_python_module(PluginContext *context)
{
  Plugin *conflicting_plugins = NULL;

#if SYSLOG_NG_ENABLE_PYTHONv2
  conflicting_plugins = (Plugin[])
  {
    PYTHON_PLUGINS(3)
  };
#elif SYSLOG_NG_ENABLE_PYTHONv3
  conflicting_plugins = (Plugin[])
  {
    PYTHON_PLUGINS(2)
  };
#else
  g_assert_not_reached();
#endif

  return plugin_is_registered(context, conflicting_plugins[0].type, conflicting_plugins[0].name);
}

static void
_set_python_path(void)
{
  const gchar *current_python_path = getenv("PYTHONPATH");
  GString *python_path = g_string_new(get_installation_path_for(SYSLOG_NG_PYTHON_MODULE_DIR));

  if (current_python_path)
    g_string_append_printf(python_path, ":%s", current_python_path);

  setenv("PYTHONPATH", python_path->str, 1);

  g_string_free(python_path, TRUE);
}

static void
_py_init_interpreter(void)
{
  if (!interpreter_initialized)
    {
      python_debugger_append_inittab();

      py_setup_python_home();
      _set_python_path();
      Py_Initialize();
      py_init_argv();

      PyEval_InitThreads();
      py_log_message_init();
      py_log_template_init();
      py_integer_pointer_init();
      py_log_source_init();
      py_log_fetcher_init();
      py_global_code_loader_init();
      py_logger_init();
      PyEval_SaveThread();

      interpreter_initialized = TRUE;
    }
}

gboolean
python_module_init(PluginContext *context, CfgArgs *args G_GNUC_UNUSED)
{
  if (_check_conflicting_python_module(context))
    {
      msg_error("Conflicting Python modules, Python 2 and Python 3 cannot be used in the same syslog-ng instance");
      return FALSE;
    }

  _py_init_interpreter();
  python_debugger_init();
  plugin_register(context, python_plugins, G_N_ELEMENTS(python_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "python",
  .version = SYSLOG_NG_VERSION,
  .description = "The python ("PYTHON_MODULE_VERSION") module provides Python scripting support for syslog-ng.",
  .core_revision = VERSION_CURRENT_VER_ONLY,
  .plugins = python_plugins,
  .plugins_len = G_N_ELEMENTS(python_plugins),
};
