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
#include "python-types.h"
#include "python-main.h"
#include "template/simple-function.h"
#include "python-logmsg.h"
#include "scratch-buffers.h"
#include <time.h>

typedef struct _PythonTfState
{
  TFSimpleFuncState super;
  GlobalConfig *cfg;
} PythonTfState;

static PyObject *
_py_construct_args_tuple(PythonTfState *state, LogMessage *msg, gint argc, GString *const *argv)
{
  PyObject *args;
  gint i;

  args = PyTuple_New(1 + argc - 1);

  PyObject *py_msg = py_log_message_new(msg, state->cfg);
  PyTuple_SetItem(args, 0, py_msg);
  for (i = 1; i < argc; i++)
    {
      PyTuple_SetItem(args, i, PyBytes_FromString(argv[i]->str));
    }
  return args;
}

/* returns NULL or reference, exception is handled */
static PyObject *
_py_invoke_template_function(PythonTfState *state, const gchar *function_name, LogMessage *msg, gint argc,
                             GString *const *argv)
{
  PyObject *callable, *ret, *args;

  callable = _py_resolve_qualified_name(function_name);
  if (!callable)
    {
      gchar buf[256];

      msg_error("$(python): Error looking up Python function",
                evt_tag_str("function", function_name),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return NULL;
    }

  msg_debug("$(python): Invoking Python template function",
            evt_tag_str("function", function_name),
            evt_tag_msg_reference(msg));

  args = _py_construct_args_tuple(state, msg, argc, argv);
  ret = PyObject_CallObject(callable, args);
  Py_DECREF(args);
  Py_DECREF(callable);

  if (!ret)
    {
      gchar buf[256];

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
_py_convert_return_value_to_result(PythonTfState *state, const gchar *function_name, PyObject *ret, GString *result,
                                   LogMessageValueType *type)
{
  if (cfg_is_config_version_older(state->cfg, VERSION_VALUE_4_0) && !is_py_obj_bytes_or_string_type(ret))
    {
      msg_error("$(python): The current config version does not support returning non-string values from Python "
                "functions. Please return str or bytes values from your Python function, use an explicit syslog-ng "
                "level cast to string() or set the config version to post 4.0",
                evt_tag_str("function", function_name),
                cfg_format_config_version_tag(state->cfg));
      Py_DECREF(ret);
      return FALSE;
    }

  ScratchBuffersMarker marker;
  GString *value = scratch_buffers_alloc_and_mark(&marker);
  if (!py_obj_to_log_msg_value(ret, value, type))
    {
      gchar buf[256];

      msg_error("$(python): error converting the return value of a Python template function to a typed name-value pair",
                evt_tag_str("function", function_name),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();

      scratch_buffers_reclaim_marked(marker);
      Py_DECREF(ret);
      return FALSE;
    }

  g_string_append(result, value->str);
  scratch_buffers_reclaim_marked(marker);

  Py_DECREF(ret);
  return TRUE;
}

static gboolean
tf_python_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[],
                  GError **error)
{
  PythonTfState *state = (PythonTfState *) s;
  state->cfg = parent->cfg;
  return tf_simple_func_prepare(self, s, parent, argc, argv, error);
}

static void
tf_python_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result,
               LogMessageValueType *type)
{
  PythonTfState *state = (PythonTfState *) s;
  PyGILState_STATE gstate;
  const gchar *function_name;
  PyObject *ret;
  LogMessage *msg = args->messages[args->num_messages-1];

  if (state->super.argc == 0)
    return;
  function_name = args->argv[0]->str;

  gstate = PyGILState_Ensure();

  if (!(ret = _py_invoke_template_function(state, function_name, msg, state->super.argc, args->argv)) ||
      !_py_convert_return_value_to_result(state, function_name, ret, result, type))
    {
      g_string_append(result, "<error>");
      *type = LM_VT_STRING;
    }

  PyGILState_Release(gstate);
}

TEMPLATE_FUNCTION(PythonTfState, tf_python, tf_python_prepare, tf_simple_func_eval,
                  tf_python_call, tf_simple_func_free_state, NULL);
