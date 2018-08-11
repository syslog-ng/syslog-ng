/*
 * Copyright (c) 2018 Balabit
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
#include "python-logtemplate.h"
#include "python-logtemplate-options.h"
#include "apphook.h"
#include "logmsg/logmsg.h"
#include "syslog-format.h"

#include <criterion/criterion.h>

static MsgFormatOptions parse_options;

static PyObject *_python_main;
static PyObject *_python_main_dict;

LogTemplateOptions log_template_options;
PyObject *py_template_options;

static void
_py_init_interpreter(void)
{
  Py_Initialize();
  py_init_argv();

  PyEval_InitThreads();
  py_log_message_init();
  py_log_template_init();
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

void setup(void)
{
  app_startup();
  log_template_options_defaults(&log_template_options);
  log_template_options.ts_format = TS_FMT_BSD;
  log_template_options.time_zone_info[LTZ_SEND]=time_zone_info_new("+05:00");

  py_template_options = py_log_template_options_new(&log_template_options);
  msg_format_options_defaults(&parse_options);
  _py_init_interpreter();
  _init_python_main();
}

void teardown(void)
{
  Py_DECREF(py_template_options);
  app_shutdown();
}

TestSuite(python_log_logtemplate, .init = setup, .fini = teardown);

static PyLogMessage *
create_parsed_message(const gchar *raw_msg)
{
  LogMessage *msg = log_msg_new_empty();
  MsgFormatOptions template_parse_options = {0};
  msg_format_options_defaults(&template_parse_options);
  syslog_format_handler(&template_parse_options, (const guchar *)raw_msg, strlen(raw_msg), msg);

  PyLogMessage *py_log_msg = (PyLogMessage *)py_log_message_new(msg);
  log_msg_unref(msg);

  return py_log_msg;
}

static PyLogTemplate *
create_py_log_template(const gchar *template)
{
  PyObject *template_str = _py_string_from_string(template, -1);
  PyObject *args = PyTuple_Pack(1, template_str);
  PyLogTemplate *py_template = (PyLogTemplate *)py_log_template_new(&py_log_template_type, args, NULL);
  Py_DECREF(template_str);
  Py_DECREF(args);

  return py_template;
}

void
assert_format(gchar *expected, PyLogTemplate *template, PyLogMessage *msg)
{

  PyObject *args = PyTuple_Pack(2, msg,  py_template_options);
  PyObject *result = py_log_template_format((PyObject *)template, args, NULL);
  Py_DECREF(args);

  cr_assert(result);
  cr_assert_str_eq(_py_get_string_as_string(result), expected);
  Py_DECREF(result);
}

Test(python_log_logtemplate, test_python_template)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyLogMessage *py_log_msg = create_parsed_message("<38>2018-07-20T00:00:00+00:00 localhost prg00000[1234]: test\n");
  PyLogTemplate *py_template = create_py_log_template("${PROGRAM}");
  cr_assert(py_template);
  assert_format("prg00000", py_template, py_log_msg);

  Py_DECREF(py_log_msg);
  Py_DECREF(py_template);
  PyGILState_Release(gstate);
}

Test(python_log_logtemplate, format_all_parameters)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyLogMessage *py_log_msg = create_parsed_message("<38>2018-07-20T00:00:00+00:00 localhost prg00000[1234]: test\n");
  PyLogTemplate *py_template = create_py_log_template("${S_STAMP} | ${SEQNUM}");
  cr_assert(py_template);
  PyObject *args = PyTuple_Pack(4, py_log_msg, py_template_options, int_as_pyobject(1), int_as_pyobject(10));
  PyObject *result = py_log_template_format((PyObject *)py_template, args, NULL);
  Py_DECREF(args);

  cr_assert(result);
  cr_assert_str_eq(_py_get_string_as_string(result), "Jul 20 05:00:00 | 10");
  Py_DECREF(result);


  Py_DECREF(py_log_msg);
  Py_DECREF(py_template);
  PyGILState_Release(gstate);
}

Test(python_log_logtemplate, test_logtemplate_exception)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyLogTemplate *py_template = create_py_log_template("${incomplete");
  cr_assert_not(py_template);

  gchar buf[256];
  _py_format_exception_text(buf, sizeof(buf));

  cr_assert(g_strstr_len(buf, sizeof(buf), "LogTemplateException"), "Wrong exception type: %s", buf);
  cr_assert(g_strstr_len(buf, sizeof(buf), "Error compiling template"), "Wrong exception string: %s", buf);

  PyGILState_Release(gstate);

}

Test(python_log_logtemplate, test_py_is_log_template_options)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  cr_assert(py_is_log_template_options((PyObject *)py_template_options));

  PyObject *template_str = _py_string_from_string("${PROGRAM}", -1);
  cr_assert_not(py_is_log_template_options((PyObject *)template_str));

  PyObject *args = PyTuple_Pack(2, template_str, Py_None); /* Second argument must be PyLogTemplateOptions */
  PyLogTemplate *py_template = (PyLogTemplate *)py_log_template_new(&py_log_template_type, args, NULL);
  Py_DECREF(template_str);
  Py_DECREF(args);
  cr_assert_null(py_template);

  gchar buf[256];
  _py_format_exception_text(buf, sizeof(buf));
  cr_assert(g_strstr_len(buf, sizeof(buf), "TypeError"), "Wrong exception : %s", buf);

  PyGILState_Release(gstate);

}
