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
#include "python-config.h"
#include "python-options.h"
#include "python-main.h"
#include "apphook.h"
#include "cfg.h"
#include "scratch-buffers.h"

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
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(python_options, .init = setup, .fini = teardown);

static void
_assert_python_option(const PythonOption *option, const gchar *expected_name, const gchar *expected_value)
{
  cr_assert_str_eq(python_option_get_name(option), expected_name);

  PyGILState_STATE gstate = PyGILState_Ensure();
  {
    PyObject *value = python_option_create_value_py_object(option);
    PyDict_SetItemString(_python_main_dict, "test_variable", value);

    gchar *script = g_strdup_printf("assert test_variable == %s", expected_value);
    if (!PyRun_String(script, Py_file_input, _python_main_dict, _python_main_dict))
      {
        PyErr_Print();
        Py_DECREF(value);
        PyGILState_Release(gstate);
        cr_assert(FALSE, "Error running Python script >>>%s<<<", script);
      }

    g_free(script);
    Py_DECREF(value);
  }
  PyGILState_Release(gstate);
}

Test(python_options, test_python_option_string)
{
  gchar *string = g_strdup("example-value");
  PythonOption *option = python_option_string_new("string", string);
  g_free(string);
  _assert_python_option(option, "string", "'example-value'");
  python_option_free(option);
}

Test(python_options, test_python_option_long)
{
  PythonOption *option = python_option_long_new("long", -42);
  _assert_python_option(option, "long", "-42");
  python_option_free(option);
}
