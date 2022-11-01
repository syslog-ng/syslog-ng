/*
 * Copyright (c) 2022 One Identity
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

/* this has to come first for modules which include the Python.h header */
#include "python-module.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include "python-helpers.h"
#include "python-types.h"
#include "python-tf.h"
#include "python-logmsg.h"
#include "python-logtemplate.h"
#include "python-logtemplate-options.h"
#include "python-integerpointer.h"
#include "python-main.h"
#include "apphook.h"
#include "logmsg/logmsg.h"
#include "scratch-buffers.h"
#include "libtest/cr_template.h"


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
  init_template_tests();

  CfgArgs *args = cfg_args_new();

  cfg_args_set(args, "use-virtualenv", "no");
  cfg_load_module_with_args(configuration, "python", args);
  cfg_args_unref(args);
  _init_python_main();
}

void
teardown(void)
{
  deinit_template_tests();
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(python_tf, .init = setup, .fini = teardown);

typedef struct _PyTfTestParams
{
  const gchar *value;
  LogMessageValueType type;
} PyTfTestParams;

ParameterizedTestParameters(python_tf, test_python_tf)
{
  static PyTfTestParams test_data_list[] =
  {
    {
      .value = "almafa",
      .type = LM_VT_STRING,
    },
    {
      .value = "42",
      .type = LM_VT_INTEGER,
    },
    {
      .value = "6.900000",
      .type = LM_VT_DOUBLE
    },
    {
      .value = "true",
      .type = LM_VT_BOOLEAN
    },
    {
      .value = "",
      .type = LM_VT_NULL
    },
    {
      .value = "a,b,c",
      .type = LM_VT_LIST
    },
  };

  return cr_make_param_array(PyTfTestParams, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(PyTfTestParams *params, python_tf, test_python_tf)
{
  const gchar *python_tf_implementation = "def test_python_tf(msg):\n"
                                          "    return msg['test_key']\n";
  const gchar *template = "$(python test_python_tf)";

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name_with_type(msg, "test_key", params->value, strlen(params->value), params->type);

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  {
    cr_assert(PyRun_String(python_tf_implementation, Py_file_input, _python_main_dict, _python_main_dict));

    cfg_set_version_without_validation(configuration, VERSION_VALUE_3_38);
    assert_template_format_value_and_type_msg(template, params->value, LM_VT_STRING, msg);

    cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
    assert_template_format_value_and_type_msg(template, params->value, params->type, msg);
  }
  PyGILState_Release(gstate);

  log_msg_unref(msg);
}
