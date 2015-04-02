/*
 * Copyright (c) 2015 BalaBit
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
#include "logmsg.h"

static void
_add_nv_keys_to_list(gpointer key, gpointer value, gpointer user_data)
{
  PyObject *list = (PyObject *) user_data;
  const gchar *name = (const gchar *) key;

  PyList_Append(list, PyString_FromString(name));
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


static void
python_debugger_export_internals(void)
{
  Py_InitModule("_syslogngdbg", _syslogngdbg_functions);
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
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))),
                NULL);
      goto exit;
    }
  if (!PyString_Check(ret))
    {
      msg_error("Return value from debugger fetch_command is not a string",
                evt_tag_str("function", DEBUGGER_FETCH_COMMAND),
                evt_tag_str("type", ret->ob_type->tp_name),
                NULL);
      Py_DECREF(ret);
      goto exit;
    }
  command = g_strdup(PyString_AsString(ret));
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
  python_debugger_export_internals();
  debugger_register_command_fetcher(python_fetch_debugger_command);
}
