/*
 * Copyright (c) 2023 Attila Szakacs
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
 */

#include "python-module.h"
#include "python-helpers.h"
#include "python-main.h"
#include "python-startup.h"
#include "python-global.h"
#include "python-types.h"

#include "apphook.h"
#include "cfg.h"
#include "reloc.h"

#include <criterion/criterion.h>

static PyObject *_python_main;
static PyObject *_python_main_dict;

static void
_init_python_main(void)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  {
    PythonConfig *pc = python_config_get(configuration);
    _python_main = _py_get_main_module(pc);
    _python_main_dict = PyModule_GetDict(_python_main);
  }
  PyGILState_Release(gstate);
}

void
setup(void)
{
  app_startup();

  configuration = cfg_new_snippet();
  _py_init_interpreter(FALSE);
  _init_python_main();
}

void
teardown(void)
{
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(python_reloc, .init = setup, .fini = teardown);

Test(python_reloc, test_get_installation_path_for)
{
  const gchar *c_resolution_1 = get_installation_path_for("/foo/bar");
  const gchar *c_resolution_2 = get_installation_path_for("${prefix}/foo/bar");

  const gchar *script = "from _syslogng import get_installation_path_for\n"
                        "assert get_installation_path_for(r'/foo/bar') == c_resolution_1\n"
                        "assert get_installation_path_for(r'${prefix}/foo/bar') == c_resolution_2\n";

  PyGILState_STATE gstate = PyGILState_Ensure();
  {
    PyDict_SetItemString(_python_main_dict, "c_resolution_1", py_string_from_string(c_resolution_1, -1));
    PyDict_SetItemString(_python_main_dict, "c_resolution_2", py_string_from_string(c_resolution_2, -1));

    if (!PyRun_String(script, Py_file_input, _python_main_dict, _python_main_dict))
      {
        PyErr_Print();
        cr_assert(FALSE, "Error running Python script >>>%s<<<", script);
      }
  }
  PyGILState_Release(gstate);
};
