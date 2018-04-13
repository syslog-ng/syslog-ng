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
#include "python-tf.h"
#include "python-helpers.h"
#include "python-main.h"
#include "template/simple-function.h"
#include "python-logmsg.h"
#include <time.h>

static PyObject *
_py_construct_args_tuple(LogMessage *msg, gint argc, GString *argv[])
{
  PyObject *args;
  gint i;

  args = PyTuple_New(1 + argc - 1);
  PyTuple_SetItem(args, 0, py_log_message_new(msg));
  for (i = 1; i < argc; i++)
    {
      PyTuple_SetItem(args, i, PyBytes_FromString(argv[i]->str));
    }
  return args;
}

/* returns NULL or reference, exception is handled */
static PyObject *
_py_invoke_template_function(const gchar *function_name, LogMessage *msg, gint argc, GString *argv[])
{
  PyObject *callable, *ret, *args;
  gchar buf[256];

  callable = _py_resolve_qualified_name(function_name);
  if (!callable)
    {
      msg_error("$(python): Error looking up Python function",
                evt_tag_str("function", function_name),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return NULL;
    }

  msg_debug("$(python): Invoking Python template function",
            evt_tag_str("function", function_name),
            evt_tag_printf("msg", "%p", msg));

  args = _py_construct_args_tuple(msg, argc, argv);
  ret = PyObject_CallObject(callable, args);
  Py_DECREF(args);
  Py_DECREF(callable);

  if (!ret)
    {
      msg_error("$(python): Error invoking Python function",
                evt_tag_str("function", function_name),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return NULL;
    }
  return ret;
}

/* NOTE: consumes ret */
static gboolean
_py_convert_return_value_to_result(const gchar *function_name, PyObject *ret, GString *result)
{
  if (!_py_is_string(ret))
    {
      msg_error("$(python): The return value is not str or unicode",
                evt_tag_str("function", function_name),
                evt_tag_str("type", ret->ob_type->tp_name));
      Py_DECREF(ret);
      return FALSE;
    }
  g_string_append(result, _py_get_string_as_string(ret));
  Py_DECREF(ret);
  return TRUE;
}

static void
tf_python(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  PyGILState_STATE gstate;
  const gchar *function_name;
  PyObject *ret;

  if (argc == 0)
    return;
  function_name = argv[0]->str;

  gstate = PyGILState_Ensure();

  if (!(ret = _py_invoke_template_function(function_name, msg, argc, argv)) ||
      !_py_convert_return_value_to_result(function_name, ret, result))
    {
      g_string_append(result, "<error>");
    }

  PyGILState_Release(gstate);
}

TEMPLATE_FUNCTION_SIMPLE(tf_python);
