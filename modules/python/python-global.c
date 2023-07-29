/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#include "python-global.h"
#include "python-config.h"
#include "python-main.h"
#include "python-global-code-loader.h"
#include "python-helpers.h"
#include "cfg.h"

static gboolean
_py_evaluate_global_code(PythonConfig *pc, const gchar *filename, const gchar *code)
{
  PyObject *module;

  module = _py_get_main_module(pc);
  if (!module)
    return FALSE;

  /* NOTE: this has become a tad more difficult than PyRun_SimpleString(),
   * because we want proper backtraces originating from this module.  Should
   * we only use PyRun_SimpleString(), we would not get source lines in our
   * backtraces.
   *
   * This implementation basically mimics what an import would do, compiles
   * the code as it was coming from a "virtual" file and then executes the
   * compiled code in the context of the _syslogng_main module.
   *
   * Since we attach a __loader__ to the module, that will be called when
   * the source is needed to report backtraces.  Its implementation is in
   * the python-global-code-loader.c, that simply returns the source when
   * its get_source() method is called.
   */
  PyObject *dict = PyModule_GetDict(module);
  PyDict_SetItemString(dict, "__loader__", py_global_code_loader_new(code));

  PyObject *code_object = Py_CompileString((char *) code, filename, Py_file_input);
  if (!code_object)
    {
      gchar buf[256];

      msg_error("Error compiling Python global code block",
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }
  module = PyImport_ExecCodeModuleEx("_syslogng_main", code_object, (char *) filename);
  Py_DECREF(code_object);

  if (!module)
    {
      gchar buf[256];

      msg_error("Error evaluating global Python block",
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }
  return TRUE;
}

gboolean
python_evaluate_global_code(GlobalConfig *cfg, const gchar *code, CFG_LTYPE *yylloc)
{
  PyGILState_STATE gstate;
  gchar buf[256];
  PythonConfig *pc = python_config_get(cfg);
  gboolean result;

  gstate = PyGILState_Ensure();
  g_snprintf(buf, sizeof(buf), "%s{python-global-code:%d}", cfg->filename, yylloc->first_line);
  result = _py_evaluate_global_code(pc, buf, code);
  PyGILState_Release(gstate);

  return result;
}
