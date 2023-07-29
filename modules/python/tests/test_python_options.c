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
#include "python-startup.h"
#include "apphook.h"
#include "cfg.h"
#include "scratch-buffers.h"
#include "string-list.h"

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
  python_option_unref(option);
}

Test(python_options, test_python_option_long)
{
  PythonOption *option = python_option_long_new("long", -42);
  _assert_python_option(option, "long", "-42");
  python_option_unref(option);
}

Test(python_options, test_python_option_double)
{
  PythonOption *option = python_option_double_new("double", -13.37);
  _assert_python_option(option, "double", "-13.37");
  python_option_unref(option);
}

Test(python_options, test_python_option_boolean)
{
  PythonOption *option = python_option_boolean_new("boolean", TRUE);
  _assert_python_option(option, "boolean", "True");
  python_option_unref(option);
}

Test(python_options, test_python_option_string_list)
{
  const gchar *string_array[] =
  {
    "example-value-1",
    "example-value-2",
    NULL,
  };
  GList *string_list = string_array_to_list(string_array);
  PythonOption *option = python_option_string_list_new("string-list", string_list);
  string_list_free(string_list);
  _assert_python_option(option, "string_list", "['example-value-1', 'example-value-2']");
  python_option_unref(option);
}

Test(python_options, test_python_option_template)
{
  gchar *template = g_strdup("${example-value}");
  PythonOption *option = python_option_template_new("template", template);
  g_free(template);

  cr_assert_str_eq(python_option_get_name(option), "template");

  PyGILState_STATE gstate = PyGILState_Ensure();
  {
    PyObject *value = python_option_create_value_py_object(option);
    PyDict_SetItemString(_python_main_dict, "test_variable", value);

    gchar *script = "import _syslogng\n"
                    "assert isinstance(test_variable, _syslogng.LogTemplate), type(test_variable)\n"
                    "assert str(test_variable) == '${example-value}'\n";
    if (!PyRun_String(script, Py_file_input, _python_main_dict, _python_main_dict))
      {
        PyErr_Print();
        Py_DECREF(value);
        PyGILState_Release(gstate);
        cr_assert(FALSE, "Error running Python script >>>%s<<<", script);
      }
    Py_DECREF(value);
  }
  PyGILState_Release(gstate);

  python_option_unref(option);
}

static PythonOptions *
_generate_options_test_options(void)
{
  PythonOptions *options = python_options_new();
  PythonOption *option;

  option = python_option_string_new("string", "example-value");
  python_options_add_option(options, option);
  python_option_unref(option);

  option = python_option_long_new("long", -42);
  python_options_add_option(options, option);
  python_option_unref(option);

  option = python_option_double_new("double", -13.37);
  python_options_add_option(options, option);
  python_option_unref(option);

  option = python_option_boolean_new("boolean", TRUE);
  python_options_add_option(options, option);
  python_option_unref(option);

  const gchar *string_array[] =
  {
    "example-value-1",
    "example-value-2",
    NULL,
  };
  GList *string_list = string_array_to_list(string_array);
  option = python_option_string_list_new("string-list", string_list);
  string_list_free(string_list);
  python_options_add_option(options, option);
  python_option_unref(option);

  option = python_option_template_new("template", "${example-template}");
  python_options_add_option(options, option);
  python_option_unref(option);

  PythonOptions *cloned = python_options_clone(options);
  python_options_free(options);

  return cloned;
}

Test(python_options, test_python_options)
{
  const gchar *script = "import _syslogng\n"
                        "assert options['string'] == 'example-value', 'Actual: {}'.format(options['string'])\n"
                        "assert options['long'] == -42, 'Actual: {}'.format(options['long'])\n"
                        "assert options['double'] == -13.37, 'Actual: {}'.format(options['double'])\n"
                        "assert options['boolean'] == True, 'Actual: {}'.format(options['string'])\n"
                        "assert options['string_list'] == ['example-value-1', 'example-value-2'], "
                        "'Actual: {}'.format(options['string_list'])\n"
                        "assert isinstance(options['template'], _syslogng.LogTemplate), 'Actual: {}'.format(type(options['template']))\n"
                        "assert str(options['template']) == '${example-template}', 'Actual: {}'.format(str(options['template']))\n";

  PythonOptions *options = _generate_options_test_options();
  PyObject *options_dict = python_options_create_py_dict(options);

  PyGILState_STATE gstate = PyGILState_Ensure();
  {
    PyDict_SetItemString(_python_main_dict, "options", options_dict);

    if (!PyRun_String(script, Py_file_input, _python_main_dict, _python_main_dict))
      {
        PyErr_Print();
        PyGILState_Release(gstate);
        cr_assert(FALSE, "Error running Python script >>>%s<<<", script);
      }
  }
  Py_DECREF(options_dict);
  PyGILState_Release(gstate);

  python_options_free(options);
}
