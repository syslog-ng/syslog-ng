/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
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
#include "python-main.h"
#include "python-module.h"
#include "python-helpers.h"
#include "messages.h"

/*
 * Some information about how we embed the Python interpreter:
 *  - instead of using the __main__ module, we use a separate _syslogng
 *    module as we want to replace it every time syslog-ng is reloaded
 *  - hanlding of our separate main module is implemented by this module
 *  - this separate __main__ module requires some magic though (most of it
 *    is in _py_construct_main_module(), see below.
 *  - the PyObject reference to the current main module is stored in the
 *    GlobalConfig instance (using the ModuleConfig mechanism)
 *  - the current main module is switched during config initialization, e.g.
 *    during runtime, _syslogng module refers to the main module of the current
 *    config
 *  - since _syslogng is different for each initialized syslog-ng
 *    configuration, it is not possible to use variables between reloads,
 *    _except_ by storing them in other, imported modules.
 *  - it is currently not possible to hook into the init/deinit mechanism in
 *    the global context (from Python), but it is possible for a DestDriver.
 *    It is doable also in the global context, but has not yet been done.
 */

static PyObject *
_py_construct_main_module(void)
{
  PyObject *module;
  PyObject *module_dict;
  PyObject *modules = PyImport_GetModuleDict();

  /* make sure the module registry doesn't contain our module */
  if (PyDict_DelItemString(modules, "_syslogng") < 0)
    PyErr_Clear();

  /* create a new module */
  module = PyImport_AddModule("_syslogng");
  if (!module)
    {
      gchar buf[256];

      msg_error("Error creating syslog-ng main module",
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return NULL;
    }

  /* make sure __builtins__ is there, it is normally automatically done for
   * __main__ and any imported modules */
  module_dict = PyModule_GetDict(module);
  if (PyDict_GetItemString(module_dict, "__builtins__") == NULL)
    {
      PyObject *builtins_module = PyImport_ImportModule(PYTHON_BUILTIN_MODULE_NAME);
      if (builtins_module == NULL ||
          PyDict_SetItemString(module_dict, "__builtins__", builtins_module) < 0)
        g_assert_not_reached();
      Py_XDECREF(builtins_module);
    }

  /* return a reference */
  Py_INCREF(module);
  return module;
}

/* switch the _syslogng main module to the one in a specific configuration */
void
_py_switch_main_module(PythonConfig *pc)
{
  PyObject *modules = PyImport_GetModuleDict();

  if (pc->main_module)
    {
      Py_INCREF(pc->main_module);
      PyDict_SetItemString(modules, "_syslogng", pc->main_module);
    }
  else
    {
      PyDict_DelItemString(modules, "_syslogng");
    }
}


/* get the current main module in a production context by using the Python
 * module registry instead of the GlobalConfig machinery.  This only works
 * once _py_switch_to_main_module() has been called (e.g. past init()).
 *
 * returns borrowed reference. */
PyObject *
_py_get_current_main_module(void)
{
  return PyImport_AddModule("_syslogng");
}

/* get the current main module as stored in the current GlobalConfig.
 * returns borrowed reference */
PyObject *
_py_get_main_module(PythonConfig *pc)
{
  if (!pc->main_module)
    pc->main_module = _py_construct_main_module();
  return pc->main_module;
}

gboolean
_py_evaluate_global_code(PythonConfig *pc, const gchar *code)
{
  PyObject *result;
  PyObject *module;
  PyObject *dict;

  module = _py_get_main_module(pc);
  if (!module)
    return FALSE;

  dict = PyModule_GetDict(module);
  result = PyRun_String(code, Py_file_input, dict, dict);

  if (!result)
    {
      gchar buf[256];

      msg_error("Error evaluating global Python block",
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }
  Py_XDECREF(result);
  return TRUE;
}

gboolean
python_evaluate_global_code(GlobalConfig *cfg, const gchar *code)
{
  PyGILState_STATE gstate;
  PythonConfig *pc = python_config_get(cfg);
  gboolean result;

  gstate = PyGILState_Ensure();
  result = _py_evaluate_global_code(pc, code);
  PyGILState_Release(gstate);

  return result;
}
