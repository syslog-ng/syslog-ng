/*
 * Copyright (c) 2020 One Identity
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
 *
 */

#include "python-http-header.h"
#include "python-helpers.h"

#include "driver.h"
#include "str-utils.h"
#include "list-adt.h"

#include "modules/http/http-signals.h"

#include <time.h>

#define PYTHON_HTTP_HEADER_PLUGIN "python-http-header"

struct _PythonHttpHeaderPlugin
{
  LogDriverPlugin super;

  gchar *class;
  GList *loaders;

  GList *last_headers;
  time_t last_run;
  int timeout;

  GHashTable *options;

  struct
  {
    PyObject *class;
    PyObject *instance;
    PyObject *get_headers;
  } py;
};

static gboolean
_py_append_pylist_to_list(PyObject *py_list, GList **list)
{
  const gchar *str;
  PyObject *py_str;

  if (!py_list || !PyList_Check(py_list))
    goto exit;

  Py_ssize_t len = PyList_Size(py_list);
  for (int i = 0; i < len; i++)
    {
      py_str = PyList_GetItem(py_list, i); // Borrowed reference
      if (!_py_is_string(py_str))
        goto exit;

      if (!(str = _py_get_string_as_string(py_str)))
        goto exit;

      *list = g_list_append(*list, g_strdup(str));
    }

  return TRUE;

exit:
  return FALSE;
}

static void
_py_append_str_to_pylist(gconstpointer data, gpointer user_data)
{
  PyObject *py_str = _py_string_from_string((gchar *) data, -1);
  if (!py_str)
    {
      gchar buf[256];

      msg_error("Error creating Python String object from C string",
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();

      goto exit;
    }

  PyObject *py_list = (PyObject *) user_data;
  if (PyList_Append(py_list, py_str) != 0)
    {
      gchar buf[256];

      msg_error("Error adding new item to Python List",
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
    }

exit:
  Py_XDECREF(py_str); // Even if append() succeeds, it doesn't steal the reference to py_str.
}

static PyObject *
_py_convert_list_to_pylist(List *list)
{
  PyObject *py_list = PyList_New(0);
  g_assert(py_list);

  if (list)
    list_foreach(list, _py_append_str_to_pylist, py_list);

  return py_list;
}

static gboolean
_py_attach_bindings(PythonHttpHeaderPlugin *self)
{
  PyObject *py_args = NULL;

  self->py.class = _py_resolve_qualified_name(self->class);
  if (!self->py.class)
    {
      gchar buf[256];

      msg_error("Error looking up Python class",
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();

      goto exit;
    }

  py_args = _py_create_arg_dict(self->options);
  if (!py_args)
    {
      gchar buf[256];

      msg_error("Error creating argument dictionary",
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();

      goto exit;
    }

  self->py.instance = _py_invoke_function(self->py.class, py_args, self->class, self->super.name);
  if (!self->py.instance)
    {
      gchar buf[256];

      msg_error("Error instantiating Python class",
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();

      goto exit;
    }

  self->py.get_headers = _py_get_attr_or_null(self->py.instance, "get_headers");
  if (!self->py.get_headers)
    {
      msg_error("Error initializing plugin, required method not found",
                evt_tag_str("class", self->class),
                evt_tag_str("method", "get_headers"));
    }

exit:
  Py_XDECREF(py_args);
  return self->py.get_headers != NULL;
}

static void
_py_detach_bindings(PythonHttpHeaderPlugin *self)
{
  Py_CLEAR(self->py.class);
  Py_CLEAR(self->py.instance);
  Py_CLEAR(self->py.get_headers);
}

static void
_append_str_to_list(gpointer data, gpointer user_data)
{
  List *list = (List *) user_data;
  list_append(list, data);
}

static void
_append_headers(PythonHttpHeaderPlugin *self, HttpHeaderRequestSignalData *data)
{
  PyObject *py_ret = NULL,
            *py_list = NULL,
             *py_args = NULL,
              *py_ret_list = NULL;

  time_t now = time(NULL);
  if ((now - self->last_run) < self->timeout)
    goto exit;

  // The GIL also guards non-Python plugin struct members from concurrent changes below.
  PyGILState_STATE gstate = PyGILState_Ensure();

  self->timeout = 0;
  g_list_free_full(self->last_headers, g_free);
  self->last_headers = NULL;

  py_list = _py_convert_list_to_pylist(data->request_headers);
  if (!py_list)
    goto cleanup;

  py_args = Py_BuildValue("(sO)", data->request_body->str, py_list);
  if (!py_args)
    {
      gchar buf[256];

      msg_error("Error creating Python arguments",
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();

      goto cleanup;
    }

  py_ret = _py_invoke_function_with_args(self->py.get_headers, py_args, self->class, "_append_headers");
  if (!py_ret || !PyArg_ParseTuple(py_ret, "iO", &self->timeout, &py_ret_list))
    {
      gchar buf[256];

      msg_error("Invalid response returned by Python call",
                evt_tag_str("class", self->class),
                evt_tag_str("method", "get_headers"),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();

      goto cleanup;
    }

  msg_debug("Python call returned valid response",
            evt_tag_str("class", self->class),
            evt_tag_str("method", "get_headers"),
            evt_tag_str("return_type", "Tuple"),
            evt_tag_int("len", PyTuple_Size(py_ret)));

  if (!_py_append_pylist_to_list(py_ret_list, &self->last_headers))
    {
      gchar buf[256];

      msg_error("Converting Python List failed",
                evt_tag_str("class", self->class),
                evt_tag_str("method", "get_headers"),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
    }

cleanup:
  Py_XDECREF(py_args);
  Py_XDECREF(py_ret);
  Py_XDECREF(py_list);
  Py_XDECREF(py_ret_list);

  self->last_run = now;

  PyGILState_Release(gstate);

exit:
  g_list_foreach(self->last_headers, _append_str_to_list, data->request_headers);
}

static gboolean
_init(PythonHttpHeaderPlugin *self)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  _py_perform_imports(self->loaders);

  if (!_py_attach_bindings(self))
    goto fail;

  PyGILState_Release(gstate);
  return TRUE;

fail:
  PyGILState_Release(gstate);
  return FALSE;
}

static gboolean
_attach(LogDriverPlugin *s, LogDriver *driver)
{
  PythonHttpHeaderPlugin *self = (PythonHttpHeaderPlugin *) s;

  if (!_init(self))
    {
      msg_error("Plugin initialization failed",
                evt_tag_str("plugin", PYTHON_HTTP_HEADER_PLUGIN));

      return FALSE;
    }

  SignalSlotConnector *ssc = driver->super.signal_slot_connector;

  CONNECT(ssc, signal_http_header_request, _append_headers, self);

  msg_debug("SignalSlotConnector slot registered",
            evt_tag_printf("signal", "%p", ssc),
            evt_tag_printf("plugin_name", "%s", PYTHON_HTTP_HEADER_PLUGIN),
            evt_tag_printf("plugin_instance", "%p", s));

  return TRUE;
}

static void
_free(LogDriverPlugin *s)
{
  PythonHttpHeaderPlugin *self = (PythonHttpHeaderPlugin *) s;

  g_free(self->class);

  if (self->options)
    g_hash_table_unref(self->options);

  if (self->loaders)
    g_list_free_full(self->loaders, g_free);

  if (self->last_headers)
    g_list_free_full(self->last_headers, g_free);

  PyGILState_STATE gstate = PyGILState_Ensure();
  _py_detach_bindings(self);
  PyGILState_Release(gstate);

  log_driver_plugin_free_method(s);
}

PythonHttpHeaderPlugin *
python_http_header_new(void)
{
  PythonHttpHeaderPlugin *self = g_new0(PythonHttpHeaderPlugin, 1);

  log_driver_plugin_init_instance(&(self->super), PYTHON_HTTP_HEADER_PLUGIN);

  self->last_headers = NULL;
  self->last_run = 0;
  self->timeout = 0;
  self->options = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  self->py.class = self->py.instance = self->py.get_headers = NULL;

  self->super.attach = _attach;
  self->super.free_fn = _free;

  return self;
}

void
python_http_header_set_loaders(PythonHttpHeaderPlugin *self, GList *loaders)
{
  g_list_free_full(self->loaders, g_free);
  self->loaders = loaders;
}

void
python_http_header_set_class(PythonHttpHeaderPlugin *self, gchar *class)
{
  g_free(self->class);
  self->class = g_strdup(class);
}

void
python_http_header_set_option(PythonHttpHeaderPlugin *self, gchar *key, gchar *value)
{
  gchar *normalized_key = __normalize_key(key);
  g_hash_table_insert(self->options, normalized_key, g_strdup(value));
}
