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

#include "python-helpers.h"
#include "python-logmsg.h"
#include <criterion/criterion.h>
#include "apphook.h"
#include "msg-format.h"
#include "logmsg/logmsg.h"

static MsgFormatOptions parse_options;

static PyObject *_python_main;
static PyObject *_python_main_dict;

static void
_py_init_interpreter(void)
{
  Py_Initialize();
  py_init_argv();

  PyEval_InitThreads();
  python_log_message_init();
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
  PyObject *res_obj = PyObject_Str(PyDict_GetItemString(dict, key));
  if (!res_obj)
    return NULL;
  gchar *res = g_strdup(py_object_as_string(res_obj));
  Py_XDECREF(res_obj);
  return res;
}

void setup(void)
{
  app_startup();
  msg_format_options_defaults(&parse_options);
  _py_init_interpreter();
  _init_python_main();
}

void teardown(void)
{
  app_shutdown();
}

TestSuite(python_log_message, .init = setup, .fini = teardown);

Test(python_log_message, test_python_logmessage_set_value)
{
  const gchar *raw_msg = "test_msg";
  LogMessage *msg = log_msg_new(raw_msg, strlen(raw_msg), NULL, &parse_options);

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
  const gchar *raw_msg = "test_msg";
  const gchar *test_value = "test_value";
  const gchar *test_key = "test_key";
  LogMessage *msg = log_msg_new(raw_msg, strlen(raw_msg), NULL, &parse_options);
  NVHandle test_key_handle = log_msg_get_value_handle(test_key);
  log_msg_set_value(msg, test_key_handle, test_value, -1);

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  {
    PyObject *msg_object = py_log_message_new(msg);

    PyDict_SetItemString(_python_main_dict, "test_msg", msg_object);

    gssize len = strlen(test_value);
    NVHandle indirect_key_handle = log_msg_get_value_handle("indirect");
    log_msg_set_value_indirect(msg, indirect_key_handle, test_key_handle, 0, 0, len);

    const gchar *get_value_indirect = "indirect = test_msg['indirect']";
    PyRun_String(get_value_indirect, Py_file_input, _python_main_dict, _python_main_dict);

    gchar *res_indirect = _dict_clone_value(_python_main_dict, "indirect");
    cr_assert_str_eq(res_indirect, "test_value");
    g_free(res_indirect);

    Py_XDECREF(msg_object);
  }
  PyGILState_Release(gstate);
}
