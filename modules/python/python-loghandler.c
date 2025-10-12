/*
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Kokan
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

#include "python-loghandler.h"

#include "messages.h"
#include "python-helpers.h"

typedef struct _PyLogLevels
{
  glong error;
  glong warning;
  glong info;
  glong debug;
  glong trace;
} PyLogLevels;

static PyLogLevels py_log_levels;
static PyType_Spec py_handler_spec;

static glong _py_fetch_log_level(PyObject *obj, const gchar *level)
{
  PyObject *py_level_value = NULL;
  // Log Levels must have positive integer values. Negative values can be used
  // to indicate a error.
  glong level_value = -1;

  py_level_value = PyObject_GetAttrString(obj, level);
  if (!py_level_value)
    {
      gchar buf[256];
      msg_error(
        "could not find log level as object attribute",
        evt_tag_str("level", level),
        evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      goto exit;
    }

  level_value = PyLong_AsLong(py_level_value);

  if (PyErr_Occurred())
    {
      gchar buf[256];
      msg_error(
        "failed to parse log level constant as glong",
        evt_tag_str("level", level),
        evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      goto exit;
    }

exit:
  Py_XDECREF(py_level_value);
  return level_value;
}

static void _py_add_logging_level(PyObject* m, const gchar* level_name, glong level_value)
{
  PyObject *add_level_function = NULL;
  PyObject *add_level_retval = NULL;

  msg_debug("registering new logging level with addLevelName()", evt_tag_str("level_name", level_name),
            evt_tag_long("level_value", level_value));

  add_level_function = _py_get_attr_or_null(m, "addLevelName");
  if (!add_level_function)
    {
      msg_error("failed to add new logging level with addLevelName(): missing method", evt_tag_str("level_name", level_name),
                evt_tag_long("level_value", level_value));
      goto exit;
    }

  add_level_retval = PyObject_CallFunction(add_level_function, "(ls)", level_value, level_name);
  if (PyErr_Occurred())
    {
      gchar buf[256];
      msg_error("failed to add new logging level with addLevelName(): method raised exception", evt_tag_str("exception",
                _py_format_exception_text(buf, sizeof(buf))), evt_tag_str("level_name", level_name), evt_tag_long("level_value",
                    level_value));
      _py_finish_exception_handling();
    }

exit:
  Py_XDECREF(add_level_function);
  Py_XDECREF(add_level_retval);
}

int py_loghandler_init(PyObject *self, PyObject *args, PyObject *kwds)
{
  int ret = -1;
  PyObject *logging_module = NULL;
  PyObject *super = NULL;
  PyObject *init_function = NULL;

  logging_module = _py_do_import("logging");
  if (!logging_module)
    return ret;

  // super(InternalHandler, self)
  super = PyObject_CallFunctionObjArgs(
            (PyObject *)&PySuper_Type, (PyObject *)Py_TYPE(self), self, NULL);
  if (!super)
    goto exit;

  init_function = _py_get_attr_or_null(super, "__init__");
  if (!init_function)
    goto exit;

  glong level_value;
  if (trace_flag)
    level_value = py_log_levels.trace;
  else if (debug_flag)
    level_value = py_log_levels.debug;
  else
    level_value = py_log_levels.info;

  PyObject *init_ret = PyObject_CallFunction(init_function, "(l)", level_value);
  ret = init_ret ? 0 : -1;
  Py_DECREF(init_ret);

exit:
  Py_XDECREF(logging_module);
  Py_XDECREF(super);
  Py_XDECREF(init_function);

  return ret;
}

PyObject *py_loghandler_emit(PyObject *self, PyObject *args)
{
  PyObject *formatted = NULL;

  PyObject *record;
  if (!PyArg_ParseTuple(args, "O", &record))
    return NULL;

  formatted = PyObject_CallMethod(self, "format", "(O)", record);
  if (!formatted)
    goto exit_error;

  const char *message = PyUnicode_AsUTF8AndSize(formatted, NULL);
  if (!message)
    goto exit_error;

  glong level = _py_fetch_log_level(record, "levelno");
  if (level < 0)
    goto exit_normal;

  if (level >= py_log_levels.error)
    msg_error(message);
  else if (level >= py_log_levels.warning)
    msg_warning(message);
  else if (level >= py_log_levels.info)
    msg_info(message);
  else if (level >= py_log_levels.debug && debug_flag)
    msg_debug(message);
  else if (level >= py_log_levels.trace && trace_flag)
    msg_trace(message);

exit_normal:
  Py_XDECREF(formatted);
  Py_RETURN_NONE;

exit_error:
  Py_XDECREF(formatted);
  return NULL;
}

static PyMethodDef py_handler_methods[] =
{
  {"emit", (PyCFunction)py_loghandler_emit, METH_VARARGS, "emit a log record"},
  {NULL}
};

static PyType_Slot py_handler_slots[] =
{
  {Py_tp_doc,     "Those messages can be captured by internal() source."},
  {Py_tp_new,     PyType_GenericNew},
  {Py_tp_init, (initproc)py_loghandler_init},
  {Py_tp_dealloc,  (destructor)py_slng_generic_dealloc},
  {Py_tp_methods, py_handler_methods},
  {0, 0}
};

static PyType_Spec py_handler_spec =
{
  .name = "InternalHandler",
  .basicsize = sizeof(PyObject),
  .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .slots = py_handler_slots,
};

void py_loghandler_global_init(void)
{
  PyObject *logging_module = NULL;
  PyObject *bases = NULL;

  logging_module = _py_do_import("logging");
  if (!logging_module)
    goto exit;

  // Handler class is fetched here only to be assigned as tp_base later (which
  // expects strong reference), so cleanup of this pointer os not required.
  PyObject *handler = PyObject_GetAttrString(logging_module, "Handler");
  if (!handler)
    {
      gchar buf[256];
      msg_error(
        "could not find logging.Handler class",
        evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      goto exit;
    }

  bases = PyTuple_Pack(1, handler);
  if (!bases)
    {
      Py_XDECREF(handler);
      goto exit;
    }

  PyObject *internal_handler = PyType_FromSpecWithBases(&py_handler_spec, bases);
  if (!internal_handler)
    goto exit;

  PyObject *m = PyImport_AddModule("_syslogng");
  PyModule_AddObject(m, "InternalHandler", internal_handler);

  py_log_levels.debug = _py_fetch_log_level(logging_module, "DEBUG");
  if (py_log_levels.debug < 0)
    goto exit;

  py_log_levels.trace = py_log_levels.debug / 2;
  if (py_log_levels.trace == py_log_levels.debug)
    {
      // Generally log levels are defined with gaps, so the above formula should
      // work. This is just a safeguard to avoid incorrect configuration.
      msg_error("DEBUG and TRACE levels received equal values",
                evt_tag_long("debug_level", py_log_levels.trace),
                evt_tag_long("trace_level", py_log_levels.trace));
      goto exit;
    }

  PyModule_AddIntConstant(m, "TRACE", py_log_levels.trace);

  _py_add_logging_level(logging_module, "TRACE", py_log_levels.trace);

  py_log_levels.error = _py_fetch_log_level(logging_module, "ERROR");
  py_log_levels.warning = _py_fetch_log_level(logging_module, "WARNING");
  py_log_levels.info = _py_fetch_log_level(logging_module, "INFO");

exit:
  Py_XDECREF(logging_module);
  Py_XDECREF(bases);
}
