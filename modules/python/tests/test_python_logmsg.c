/*
 * Copyright (c) 2017 Balabit
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
#include "libtest/msg_parse_lib.h"

#include "python-helpers.h"
#include "python-types.h"
#include "python-logmsg.h"
#include "python-main.h"
#include "python-startup.h"
#include "apphook.h"
#include "logmsg/logmsg.h"
#include "scratch-buffers.h"

static PyObject *_python_main;
static PyObject *_python_main_dict;

MsgFormatOptions parse_options;

static void
_init_python_main(void)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  {
    PythonConfig *pc = python_config_get(configuration);
    _python_main = _py_get_main_module(pc);
    _python_main_dict = PyModule_GetDict(_python_main);
    g_assert(PyRun_String("import datetime", Py_file_input, _python_main_dict, _python_main_dict));
  }
  PyGILState_Release(gstate);
}

static void
_assert_python_variable_value(const gchar *variable_name, const gchar *expected_value)
{
  gchar *script = g_strdup_printf("assert %s == %s", variable_name, expected_value);
  if (!PyRun_String(script, Py_file_input, _python_main_dict, _python_main_dict))
    {
      gchar buf[256];

      msg_error("Error in _assert_python_variable_value()",
                evt_tag_str("script", script),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();

      g_free(script);
      cr_assert(FALSE);
    }

  g_free(script);
}

static PyLogMessage *
_construct_py_log_msg(PyObject *args)
{
  PyLogMessage *py_msg = (PyLogMessage *) PyObject_CallFunctionObjArgs((PyObject *) &py_log_message_type, args, NULL);
  cr_assert_not_null(py_msg);

  return (PyLogMessage *) py_msg;

}

static PyObject *
_construct_py_parse_options(void)
{
  PyObject *py_parse_options = PyCapsule_New(&parse_options, NULL, NULL);

  cr_assert_not_null(py_parse_options);

  return py_parse_options;
}

void
setup(void)
{
  setenv("TZ", "UTC", TRUE);
  app_startup();

  init_parse_options_and_load_syslogformat(&parse_options);

  _py_init_interpreter(FALSE);
  _init_python_main();
}

void
teardown(void)
{
  scratch_buffers_explicit_gc();
  deinit_syslogformat_module();
  app_shutdown();
}

TestSuite(python_log_message, .init = setup, .fini = teardown);

typedef struct _PyLogMessageSetValueTestParams
{
  const gchar *py_value_to_set;
  const gchar *py_value_to_check;
  const gchar *expected_log_msg_value;
  LogMessageValueType expected_log_msg_type;
} PyLogMessageSetValueTestParams;

ParameterizedTestParameters(python_log_message, test_python_logmessage_set_value)
{
  static PyLogMessageSetValueTestParams test_data_list[] =
  {
    {
      .py_value_to_set = "'almafa'",
      .py_value_to_check = "b'almafa'",
      .expected_log_msg_value = "almafa",
      .expected_log_msg_type = LM_VT_STRING
    },
    {
      .py_value_to_set = "b'kortefa'",
      .py_value_to_check = "b'kortefa'",
      .expected_log_msg_value = "kortefa",
      .expected_log_msg_type = LM_VT_STRING
    },
    {
      .py_value_to_set = "42",
      .py_value_to_check = "42",
      .expected_log_msg_value = "42",
      .expected_log_msg_type = LM_VT_INTEGER
    },
    {
      .py_value_to_set = "6.9",
      .py_value_to_check = "6.9",
      .expected_log_msg_value = "6.900000",
      .expected_log_msg_type = LM_VT_DOUBLE
    },
    {
      .py_value_to_set = "True",
      .py_value_to_check = "True",
      .expected_log_msg_value = "true",
      .expected_log_msg_type = LM_VT_BOOLEAN
    },
    {
      .py_value_to_set = "None",
      .py_value_to_check = "None",
      .expected_log_msg_value = "",
      .expected_log_msg_type = LM_VT_NULL
    },
    {
      .py_value_to_set = "['a,', ' b', b'c']",
      .py_value_to_check = "[b'a,', b' b', b'c']",
      .expected_log_msg_value = "\"a,\",\" b\",c",
      .expected_log_msg_type = LM_VT_LIST
    },
    {
      .py_value_to_set = "datetime.datetime(2022, 10, 4, 14, 48, 12, 123000)",
      .py_value_to_check = "datetime.datetime(2022, 10, 4, 14, 48, 12, 123000)",
      .expected_log_msg_value = "1664894892.123",
      .expected_log_msg_type = LM_VT_DATETIME
    },
  };

  return cr_make_param_array(PyLogMessageSetValueTestParams, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(PyLogMessageSetValueTestParams *params, python_log_message, test_python_logmessage_set_value)
{
  LogMessage *msg = log_msg_new_empty();

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  {
    cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
    PyObject *msg_object = py_log_message_new(msg, configuration);

    PyDict_SetItemString(_python_main_dict, "test_msg", msg_object);

    gchar *script = g_strdup_printf(
                      "test_msg['test_field'] = %s\n"
                      "result = test_msg.get('test_field')\n",
                      params->py_value_to_set
                    );
    if (!PyRun_String(script, Py_file_input, _python_main_dict, _python_main_dict))
      {
        PyErr_Print();
        cr_assert(FALSE, "Error running Python script >>>%s<<<", script);
      }

    g_free(script);

    _assert_python_variable_value("result", params->py_value_to_check);

    LogMessageValueType type;
    const gchar *value = log_msg_get_value_by_name_with_type(msg, "test_field", NULL, &type);
    cr_assert_str_eq(value, params->expected_log_msg_value);
    cr_assert(type == params->expected_log_msg_type);

    Py_XDECREF(msg_object);
  }
  PyGILState_Release(gstate);
}

static void
_run_scripts(const gchar *script)
{
  if (!PyRun_String(script, Py_file_input, _python_main_dict, _python_main_dict))
    {
      PyErr_Print();
      cr_assert(FALSE, "Error running Python script >>>%s<<<", script);
    }
}

Test(python_log_message, test_python_logmessage_subscript)
{
  LogMessage *msg = log_msg_new_empty();

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  {
    cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
    PyObject *msg_object = py_log_message_new(msg, configuration);
    PyDict_SetItemString(_python_main_dict, "test_msg", msg_object);

    log_msg_set_value_by_name_with_type(msg, "field", "25", -1, LM_VT_INTEGER);

    _run_scripts("result = test_msg['field']");
    _assert_python_variable_value("result", "25");

    cr_assert_null(PyRun_String("result = test_msg['nonexistent']", Py_file_input,
                                _python_main_dict, _python_main_dict));
    cr_assert_not_null(PyErr_Occurred());
    cr_assert(PyErr_ExceptionMatches(PyExc_KeyError));

    Py_XDECREF(msg_object);
  }
  PyGILState_Release(gstate);
}

Test(python_log_message, test_python_logmessage_subscript_no_typing_support)
{
  LogMessage *msg = log_msg_new_empty();

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  {
    cfg_set_version_without_validation(configuration, VERSION_VALUE_3_38);
    PyObject *msg_object = py_log_message_new(msg, configuration);
    PyDict_SetItemString(_python_main_dict, "test_msg", msg_object);

    log_msg_set_value_by_name_with_type(msg, "field", "25", -1, LM_VT_INTEGER);

    _run_scripts("result = test_msg['field']");
    _assert_python_variable_value("result", "b'25'");

    _run_scripts("result = test_msg['nonexistent']");
    _assert_python_variable_value("result", "b''");

    Py_XDECREF(msg_object);
  }
  PyGILState_Release(gstate);
}

typedef struct
{
  const gchar *value;
  gssize value_length;
  LogMessageValueType type;
  const gchar *expected_value;
} PyLogMessageGetTestParams;

ParameterizedTestParameters(python_log_message, test_python_logmessage_get)
{
  static PyLogMessageGetTestParams test_data_list[] =
  {
    {
      .value = "test",
      .value_length = -1,
      .type = LM_VT_STRING,
      .expected_value = "b'test'"
    },
    {
      .value = "true",
      .value_length = -1,
      .type = LM_VT_BOOLEAN,
      .expected_value = "True"
    },
    {
      .value = "14",
      .value_length = -1,
      .type = LM_VT_INTEGER,
      .expected_value = "14"
    },
    {
      .value = "14",
      .value_length = -1,
      .type = LM_VT_DOUBLE,
      .expected_value = "14.000"
    },
    {
      .value = "1664894892",
      .value_length = -1,
      .type = LM_VT_DATETIME,
      .expected_value = "datetime.datetime(2022, 10, 4, 14, 48, 12)"
    },
    {
      .value = "\"a,\",\" b\",c",
      .value_length = -1,
      .type = LM_VT_LIST,
      .expected_value = "[b'a,', b' b', b'c']"
    },
    {
      .value = "",
      .value_length = -1,
      .type = LM_VT_NULL,
      .expected_value = "None"
    },
    {
      .value = "\0\1\2\3",
      .value_length = 4,
      .type = LM_VT_BYTES,
      .expected_value = NULL
    },
    {
      .value = "\4\5\6\7",
      .value_length = 4,
      .type = LM_VT_PROTOBUF,
      .expected_value = NULL
    },
  };

  return cr_make_param_array(PyLogMessageGetTestParams, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(PyLogMessageGetTestParams *params, python_log_message, test_python_logmessage_get)
{
  LogMessage *msg = log_msg_new_empty();

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  {
    cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
    PyObject *msg_object = py_log_message_new(msg, configuration);
    PyDict_SetItemString(_python_main_dict, "test_msg", msg_object);

    log_msg_set_value_by_name_with_type(msg, "field", params->value, params->value_length, params->type);

    if (params->expected_value)
      {
        _run_scripts("result = test_msg.get('field')");
        _assert_python_variable_value("result", params->expected_value);

        _run_scripts("result = test_msg.get('field', default='def')");
        _assert_python_variable_value("result", params->expected_value);
      }
    else
      {
        _run_scripts("result = test_msg.get('field')");
        _assert_python_variable_value("result", "None");

        _run_scripts("result = test_msg.get('field', default='def')");
        _assert_python_variable_value("result", "'def'");
      }

    Py_XDECREF(msg_object);
  }
  PyGILState_Release(gstate);
}

Test(python_log_message, test_python_logmessage_get_with_default_value)
{
  LogMessage *msg = log_msg_new_empty();

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  {
    cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
    PyObject *msg_object = py_log_message_new(msg, configuration);
    PyDict_SetItemString(_python_main_dict, "test_msg", msg_object);

    _run_scripts("result = test_msg.get('nonexistent', default=-1)");
    _assert_python_variable_value("result", "-1");

    _run_scripts("result = test_msg.get('nonexistent')");
    _assert_python_variable_value("result", "None");

    Py_XDECREF(msg_object);
  }
  PyGILState_Release(gstate);
}

ParameterizedTestParameters(python_log_message, test_python_logmessage_get_as_str)
{
  static PyLogMessageGetTestParams test_data_list[] =
  {
    {
      .value = "test",
      .value_length = -1,
      .type = LM_VT_STRING,
      .expected_value = "'test'"
    },
    {
      .value = "true",
      .value_length = -1,
      .type = LM_VT_BOOLEAN,
      .expected_value = "'true'"
    },
    {
      .value = "14",
      .value_length = -1,
      .type = LM_VT_INTEGER,
      .expected_value = "'14'"
    },
    {
      .value = "14",
      .value_length = -1,
      .type = LM_VT_DOUBLE,
      .expected_value = "'14'"
    },
    {
      .value = "1664894892",
      .value_length = -1,
      .type = LM_VT_DATETIME,
      .expected_value = "'1664894892'"
    },
    {
      .value = "\"a,\",\" b\",c",
      .value_length = -1,
      .type = LM_VT_LIST,
      .expected_value = "'\"a,\",\" b\",c'"
    },
    {
      .value = "",
      .value_length = -1,
      .type = LM_VT_NULL,
      .expected_value = "''"
    },
    {
      .value = "\0\1\2\3",
      .value_length = 4,
      .type = LM_VT_BYTES,
      .expected_value = NULL
    },
    {
      .value = "\4\5\6\7",
      .value_length = 4,
      .type = LM_VT_PROTOBUF,
      .expected_value = NULL
    },
  };

  return cr_make_param_array(PyLogMessageGetTestParams, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(PyLogMessageGetTestParams *params, python_log_message, test_python_logmessage_get_as_str)
{
  LogMessage *msg = log_msg_new_empty();

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  {
    cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
    PyObject *msg_object = py_log_message_new(msg, configuration);
    PyDict_SetItemString(_python_main_dict, "test_msg", msg_object);

    log_msg_set_value_by_name_with_type(msg, "field", params->value, params->value_length, params->type);

    if (params->expected_value)
      {
        _run_scripts("result = test_msg.get_as_str('field')");
        _assert_python_variable_value("result", params->expected_value);

        _run_scripts("result = test_msg.get_as_str('field', default='def')");
        _assert_python_variable_value("result", params->expected_value);
      }
    else
      {
        _run_scripts("result = test_msg.get_as_str('field', )");
        _assert_python_variable_value("result", "None");

        _run_scripts("result = test_msg.get_as_str('field', default='def')");
        _assert_python_variable_value("result", "'def'");
      }

    Py_XDECREF(msg_object);
  }
  PyGILState_Release(gstate);
}

Test(python_log_message, test_python_logmessage_get_as_str_with_default_value)
{
  LogMessage *msg = log_msg_new_empty();

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  {
    cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
    PyObject *msg_object = py_log_message_new(msg, configuration);
    PyDict_SetItemString(_python_main_dict, "test_msg", msg_object);

    _run_scripts("result = test_msg.get_as_str('nonexistent', default='default', encoding='utf-8', errors='strict')");
    _assert_python_variable_value("result", "'default'");

    _run_scripts("result = test_msg.get_as_str('nonexistent')");
    _assert_python_variable_value("result", "None");

    Py_XDECREF(msg_object);
  }
  PyGILState_Release(gstate);
}

Test(python_log_message, test_python_logmessage_set_value_no_typing_support)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_3_38);

  LogMessage *msg = log_msg_new_empty();

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  {
    PyObject *msg_object = py_log_message_new(msg, configuration);

    PyDict_SetItemString(_python_main_dict, "test_msg", msg_object);

    const gchar *script = "test_msg['test_field'] = 42\n";
    cr_assert_not(PyRun_String(script, Py_file_input, _python_main_dict, _python_main_dict));

    Py_XDECREF(msg_object);
  }
  PyGILState_Release(gstate);
}

Test(python_log_message, test_python_logmessage_get_value_no_typing_support)
{
  const gchar *test_key = "test_field";
  const gchar *test_value = "42";

  cfg_set_version_without_validation(configuration, VERSION_VALUE_3_38);

  LogMessage *msg = log_msg_new_empty();

  // set from C code as INTEGER
  log_msg_set_value_by_name_with_type(msg, test_key, test_value, strlen(test_value), LM_VT_INTEGER);

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  {
    PyObject *msg_object = py_log_message_new(msg, configuration);

    PyDict_SetItemString(_python_main_dict, "test_msg", msg_object);

    // in python code it shows up as bytes
    _assert_python_variable_value("test_msg['test_field']", "b'42'");

    LogMessageValueType type;
    const gchar *value = log_msg_get_value_by_name_with_type(msg, "test_field", NULL, &type);

    // in LogMessage it is INTEGER
    cr_assert(type == LM_VT_INTEGER);
    cr_assert_str_eq(value, "42");

    Py_XDECREF(msg_object);
  }
  PyGILState_Release(gstate);
}

Test(python_log_message, test_python_logmessage_set_value_indirect)
{
  const gchar *test_value = "test_value";
  const gchar *test_key = "test_key";
  LogMessage *msg = log_msg_new_empty();
  NVHandle test_key_handle = log_msg_get_value_handle(test_key);
  log_msg_set_value(msg, test_key_handle, test_value, -1);

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  {
    PyObject *msg_object = py_log_message_new(msg, configuration);

    PyDict_SetItemString(_python_main_dict, "test_msg", msg_object);

    gssize len = strlen(test_value);
    NVHandle indirect_key_handle = log_msg_get_value_handle("indirect");
    log_msg_set_value_indirect(msg, indirect_key_handle, test_key_handle, 0, len);

    const gchar *get_value_indirect = "indirect = test_msg['indirect']";
    PyRun_String(get_value_indirect, Py_file_input, _python_main_dict, _python_main_dict);

    _assert_python_variable_value("indirect", "b'test_value'");

    Py_XDECREF(msg_object);
  }
  PyGILState_Release(gstate);
}

Test(python_log_message, test_py_is_log_message)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  LogMessage *msg = log_msg_new_empty();
  PyObject *msg_object = py_log_message_new(msg, configuration);

  cr_assert(py_is_log_message(msg_object));
  cr_assert_not(py_is_log_message(_python_main));

  log_msg_unref(msg);
  Py_DECREF(msg_object);
  PyGILState_Release(gstate);
}

Test(python_log_message, test_py_log_message_constructor_with_str)
{
  const gchar *test_str_msg = "árvíztűrőtükörfúrógép";

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyObject *arg_str = Py_BuildValue("s", test_str_msg);
  PyLogMessage *py_msg = _construct_py_log_msg(arg_str);
  Py_DECREF(arg_str);

  gssize msg_length;
  const gchar *msg = log_msg_get_value(py_msg->msg, LM_V_MESSAGE, &msg_length);

  cr_assert_eq(msg_length, strlen(test_str_msg));
  cr_assert_str_eq(msg, test_str_msg);

  Py_DECREF(py_msg);
  PyGILState_Release(gstate);
}

Test(python_log_message, test_py_log_message_constructor_with_binary)
{
  const gchar test_binary_msg[] = "űú\0\u2603\n\rő";

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyObject *arg_binary = Py_BuildValue("s#", test_binary_msg, sizeof(test_binary_msg));
  PyLogMessage *py_msg = _construct_py_log_msg(arg_binary);
  Py_DECREF(arg_binary);

  gssize msg_length;
  const gchar *msg = log_msg_get_value(py_msg->msg, LM_V_MESSAGE, &msg_length);

  cr_assert_eq(msg_length, sizeof(test_binary_msg));
  cr_assert_arr_eq(msg, test_binary_msg, sizeof(test_binary_msg));

  Py_DECREF(py_msg);
  PyGILState_Release(gstate);
}

Test(python_log_message, test_py_log_message_set_pri)
{
  gint pri = 165;

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyObject *arg = Py_BuildValue("i", pri);
  PyLogMessage *py_msg = _construct_py_log_msg(NULL);

  PyObject *ret = _py_invoke_method_by_name((PyObject *) py_msg, "set_pri", arg, NULL, NULL);
  Py_XDECREF(ret);

  cr_assert_eq(py_msg->msg->pri, pri);

  ret = _py_invoke_method_by_name((PyObject *) py_msg, "get_pri", NULL, NULL, NULL);
  pri = PyLong_AsLong(ret);
  Py_XDECREF(ret);

  cr_assert_eq(py_msg->msg->pri, pri);
  py_msg->msg->pri = 55;

  ret = _py_invoke_method_by_name((PyObject *) py_msg, "get_pri", NULL, NULL, NULL);
  pri = PyLong_AsLong(ret);
  Py_XDECREF(ret);
  cr_assert_eq(py_msg->msg->pri, pri);

  Py_DECREF(py_msg);
  Py_DECREF(arg);
  PyGILState_Release(gstate);
}

Test(python_log_message, test_py_log_message_set_timestamp)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyObject *arg = Py_BuildValue("O", py_datetime_from_msec(1664890194123));
  PyLogMessage *py_msg = _construct_py_log_msg(NULL);

  PyObject *ret = _py_invoke_method_by_name((PyObject *) py_msg, "set_timestamp", arg, NULL, NULL);
  cr_assert(ret);
  Py_XDECREF(ret);

  cr_assert_eq(py_msg->msg->timestamps[LM_TS_STAMP].ut_sec, 1664890194);
  cr_assert_eq(py_msg->msg->timestamps[LM_TS_STAMP].ut_usec, 123000);

  UnixTime ut;

  py_msg->msg->timestamps[LM_TS_STAMP].ut_sec++;
  ret = _py_invoke_method_by_name((PyObject *) py_msg, "get_timestamp", NULL, NULL, NULL);
  cr_assert(ret);
  py_datetime_to_unix_time(ret, &ut);

  cr_assert_eq(ut.ut_sec, 1664890195);
  cr_assert_eq(ut.ut_usec, 123000);
  Py_XDECREF(ret);

  Py_DECREF(py_msg);
  Py_DECREF(arg);
  PyGILState_Release(gstate);
}

Test(python_log_message, test_py_log_message_parse)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyObject *py_parse_options = _construct_py_parse_options();
  PyObject *arg = Py_BuildValue("s", "<34>Oct 11 22:14:15 mymachine su: 'su root' failed for lonvick on /dev/pts/8");

  PyObject *parse_method = _py_get_attr_or_null((PyObject *) &py_log_message_type, "parse");
  cr_assert_not_null(parse_method);

  PyLogMessage *py_msg = (PyLogMessage *) PyObject_CallFunctionObjArgs(parse_method, arg, py_parse_options, NULL);
  cr_assert_not_null(py_msg);

  Py_DECREF(parse_method);
  Py_DECREF(arg);
  Py_DECREF(py_parse_options);

  const gchar *msg = log_msg_get_value(py_msg->msg, LM_V_MESSAGE, NULL);
  cr_assert_str_eq(msg, "'su root' failed for lonvick on /dev/pts/8");

  const gchar *host = log_msg_get_value(py_msg->msg, LM_V_HOST, NULL);
  cr_assert_str_eq(host, "mymachine");

  const gchar *program = log_msg_get_value(py_msg->msg, LM_V_PROGRAM, NULL);
  cr_assert_str_eq(program, "su");

  cr_assert_eq(py_msg->msg->pri, 34);

  Py_DECREF(py_msg);
  PyGILState_Release(gstate);
}

Test(python_log_message, test_python_logmessage_keys)
{
  const gchar *expected_key = "test_key";
  LogMessage *msg = log_msg_new_internal(LOG_INFO | LOG_SYSLOG, "test");
  log_msg_set_value_by_name(msg, expected_key, "value", -1);

  log_msg_get_value_handle("unused_value");

  PyGILState_STATE gstate = PyGILState_Ensure();
  PyObject *py_msg = py_log_message_new(msg, configuration);

  PyObject *keys = _py_invoke_method_by_name(py_msg, "keys", NULL, "PyLogMessageTest", NULL);
  cr_assert_not_null(keys);
  cr_assert(PyList_Check(keys));

  for (Py_ssize_t i = 0; i < PyList_Size(keys); ++i)
    {
      PyObject *py_key = PyList_GetItem(keys, i);
      const gchar *key;
      py_bytes_or_string_to_string(py_key, &key);
      cr_assert_not_null(key);

      NVHandle handle = log_msg_get_value_handle(key);
      cr_assert(g_strcmp0(key, expected_key) == 0
                || log_msg_is_handle_macro(handle)
                || handle == LM_V_MESSAGE
                || handle == LM_V_PROGRAM
                || handle == LM_V_PID, "Unexpected key found in PyLogMessage: %s", key);
    }

  Py_XDECREF(keys);
  Py_XDECREF(py_msg);
  PyGILState_Release(gstate);
}
