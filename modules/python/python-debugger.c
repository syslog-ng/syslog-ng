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
#include "python-debugger.h"
#include "python-module.h"
#include "python-helpers.h"
#include "messages.h"
#include "debugger/debugger.h"
#include "logmsg/logmsg.h"

static void
_add_nv_keys_to_list(gpointer key, gpointer value, gpointer user_data)
{
  PyObject *list = (PyObject *) user_data;
  const gchar *name = (const gchar *) key;

  PyList_Append(list, PyBytes_FromString(name));
}

static PyObject *
_py_get_nv_registry(PyObject *s, PyObject *args)
{
  PyObject *list;

  if (!PyArg_ParseTuple(args, ""))
    return NULL;

  list = PyList_New(0);
  log_msg_registry_foreach(_add_nv_keys_to_list, list);
  return list;
}

static PyMethodDef _syslogngdbg_functions[] =
{
  { "get_nv_registry",  _py_get_nv_registry, METH_VARARGS, "Get the list of registered name-value pairs in syslog-ng" },
  { NULL,            NULL, 0, NULL }   /* sentinel*/
};

#if PY_MAJOR_VERSION >= 3

static struct PyModuleDef syslogngdbgmodule =
{
  .m_base    = PyModuleDef_HEAD_INIT,
  .m_name    = "syslogngdbg",
  .m_size    = -1,
  .m_methods = _syslogngdbg_functions
};

static PyObject *
PyInit_syslogngdbg(void)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  PyObject *module = PyModule_Create(&syslogngdbgmodule);
  PyGILState_Release(gstate);

  return module;
}

#else

static void
PyInit_syslogngdbg(void)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  Py_InitModule("syslogngdbg", _syslogngdbg_functions);
  PyGILState_Release(gstate);
}

#endif

void
python_debugger_append_inittab(void)
{
  PyImport_AppendInittab("syslogngdbg", &PyInit_syslogngdbg);
}

#define DEBUGGER_FETCH_COMMAND "syslogng.debuggercli.fetch_command"

static gchar *
python_fetch_debugger_command(void)
{
  PyObject *fetch_command;
  PyObject *ret;
  gchar *command = NULL;

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  fetch_command = _py_resolve_qualified_name(DEBUGGER_FETCH_COMMAND);
  if (!fetch_command)
    goto exit;

  ret = PyObject_CallFunctionObjArgs(fetch_command, NULL);
  if (!ret)
    {
      gchar buf[256];

      msg_error("Error calling debugger fetch_command",
                evt_tag_str("function", DEBUGGER_FETCH_COMMAND),
                evt_tag_str("exception", _py_fetch_and_format_exception_text(buf, sizeof(buf))));
      goto exit;
    }
  if (!_py_is_string(ret))
    {
      msg_error("Return value from debugger fetch_command is not a string",
                evt_tag_str("function", DEBUGGER_FETCH_COMMAND),
                evt_tag_str("type", ret->ob_type->tp_name));
      Py_DECREF(ret);
      goto exit;
    }
  command = g_strdup(_py_get_string_value_as_utf8(ret));
  Py_DECREF(ret);
exit:
  PyGILState_Release(gstate);
  if (!command)
    return debugger_builtin_fetch_command();
  return command;
}

void
python_debugger_init(void)
{
  debugger_register_command_fetcher(python_fetch_debugger_command);
}
