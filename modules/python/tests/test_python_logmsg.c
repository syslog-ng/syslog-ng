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
#include "libtest/msg_parse_lib.h"

#include "python-helpers.h"
#include "python-logmsg.h"
#include "apphook.h"
#include "logmsg/logmsg.h"

static PyObject *_python_main;
static PyObject *_python_main_dict;

MsgFormatOptions parse_options;

static void
_py_init_interpreter(void)
{
  py_setup_python_home();
  Py_Initialize();
  py_init_argv();

  py_init_threads();
  py_log_message_init();
  PyEval_SaveThread();
}

static void
_init_python_main(void)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  {
    _python_main = PyImport_AddModule("__main__");
    _python_main_dict = PyModule_GetDict(_python_main);
  }
  PyGILState_Release(gstate);
}

static gchar *
_dict_clone_value(PyObject *dict, const gchar *key)
{
  PyObject *res_obj = PyDict_GetItemString(dict, key);

  if (!res_obj)
    return NULL;

  if (!_py_is_string(res_obj))
    {
      Py_XDECREF(res_obj);
      return NULL;
    }

  gchar *res = g_strdup(_py_get_string_as_string(res_obj));
  Py_XDECREF(res_obj);
  return res;
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

void setup(void)
{
  app_startup();

  init_parse_options_and_load_syslogformat(&parse_options);

  _py_init_interpreter();
  _init_python_main();
}

void teardown(void)
{
  deinit_syslogformat_module();
  app_shutdown();
}

TestSuite(python_log_message, .init = setup, .fini = teardown);

Test(python_log_message, test_python_logmessage_set_value)
{
  LogMessage *msg = log_msg_new_empty();

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  {
    PyObject *msg_object = py_log_message_new(msg);

    PyDict_SetItemString(_python_main_dict, "test_msg", msg_object);
    const gchar *set_value = "test_msg['test_field'] = 'test_value'\nresult=test_msg['test_field']";
    PyRun_String(set_value, Py_file_input, _python_main_dict, _python_main_dict);

    gchar *res = _dict_clone_value(_python_main_dict, "result");
    cr_assert_str_eq(res, "test_value");
    g_free(res);

    const gchar *test_value=log_msg_get_value_by_name(msg, "test_field", NULL);
    cr_assert_str_eq(test_value, "test_value");

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
    PyObject *msg_object = py_log_message_new(msg);

    PyDict_SetItemString(_python_main_dict, "test_msg", msg_object);

    gssize len = strlen(test_value);
    NVHandle indirect_key_handle = log_msg_get_value_handle("indirect");
    log_msg_set_value_indirect(msg, indirect_key_handle, test_key_handle, 0, len);

    const gchar *get_value_indirect = "indirect = test_msg['indirect']";
    PyRun_String(get_value_indirect, Py_file_input, _python_main_dict, _python_main_dict);

    gchar *res_indirect = _dict_clone_value(_python_main_dict, "indirect");
    cr_assert_str_eq(res_indirect, "test_value");
    g_free(res_indirect);

    Py_XDECREF(msg_object);
  }
  PyGILState_Release(gstate);
}

Test(python_log_message, test_py_is_log_message)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  LogMessage *msg = log_msg_new_empty();
  PyObject *msg_object = py_log_message_new(msg);

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
  Py_DECREF(arg);

  PyObject *ret = _py_invoke_method_by_name((PyObject *) py_msg, "set_pri", arg, NULL, NULL);
  Py_XDECREF(ret);

  cr_assert_eq(py_msg->msg->pri, pri);

  Py_DECREF(py_msg);
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
  PyObject *py_msg = py_log_message_new(msg);

  PyObject *keys = _py_invoke_method_by_name(py_msg, "keys", NULL, "PyLogMessageTest", NULL);
  cr_assert_not_null(keys);
  cr_assert(PyList_Check(keys));

  for (Py_ssize_t i = 0; i < PyList_Size(keys); ++i)
    {
      PyObject *py_key = PyList_GetItem(keys, i);
      const gchar *key = _py_get_string_as_string(py_key);
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
