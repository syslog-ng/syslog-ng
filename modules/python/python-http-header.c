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

  gboolean mark_errors_as_critical;
  gchar *class;
  GList *loaders;

  GHashTable *options;

  struct
  {
    PyObject *class;
    PyObject *instance;
    PyObject *get_headers;
    PyObject *on_http_response_received;
  } py;
};

static gboolean
_py_append_pylist_to_list(PyObject *py_list, GList **list)
{
  const gchar *str;
  PyObject *py_str;

  if (!py_list)
    {
      msg_debug("Trying to append a NULL-valued PyList to GList");
      goto exit;
    }

  if (!PyList_Check(py_list))
    {
      msg_debug("PyList_Check failed when trying to append PyList to GList.");
      goto exit;
    }

  Py_ssize_t len = PyList_Size(py_list);
  for (int i = 0; i < len; i++)
    {
      py_str = PyList_GetItem(py_list, i); // Borrowed reference
      if (!_py_is_string(py_str))
        {
          msg_debug("PyList contained a non-string object when trying to append to GList");
          goto exit;
        }

      if (!(str = _py_get_string_as_string(py_str)))
        {
          msg_debug("_py_get_string_as_string failed when trying to append PyList to GList");
          goto exit;
        }

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
      _py_format_exception_text(buf, sizeof(buf));

      msg_error("Error creating Python String object from C string",
                evt_tag_str("exception", buf));
      _py_finish_exception_handling();

      goto exit;
    }

  PyObject *py_list = (PyObject *) user_data;
  if (PyList_Append(py_list, py_str) != 0)
    {
      gchar buf[256];
      _py_format_exception_text(buf, sizeof(buf));

      msg_error("Error adding new item to Python List",
                evt_tag_str("exception", buf));
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
_py_attach_class(PythonHttpHeaderPlugin *self)
{
  self->py.class = _py_resolve_qualified_name(self->class);
  if (!self->py.class)
    {
      gchar buf[256];
      _py_format_exception_text(buf, sizeof(buf));

      msg_error("Error looking up Python class",
                evt_tag_str("class", self->class),
                evt_tag_str("exception", buf));
      _py_finish_exception_handling();

      return FALSE;
    }
  return TRUE;
}

static PyObject *
_create_arg_dict_from_options(PythonHttpHeaderPlugin *self)
{
  PyObject *py_args = _py_create_arg_dict(self->options);

  if (!py_args)
    {
      gchar buf[256];
      _py_format_exception_text(buf, sizeof(buf));

      msg_error("Error creating argument dictionary",
                evt_tag_str("class", self->class),
                evt_tag_str("exception", buf));
      _py_finish_exception_handling();

      return NULL;
    }

  return py_args;
}

static gboolean
_py_instantiate_class(PythonHttpHeaderPlugin *self)
{
  PyObject *py_args = _create_arg_dict_from_options(self);
  if (!py_args)
    return FALSE;

  gboolean result = FALSE;
  self->py.instance = _py_invoke_function(self->py.class, py_args, self->class, self->super.name);
  if (!self->py.instance)
    {
      gchar buf[256];
      _py_format_exception_text(buf, sizeof(buf));

      msg_error("Error instantiating Python class",
                evt_tag_str("class", self->class),
                evt_tag_str("exception", buf));
      _py_finish_exception_handling();

      goto exit;
    }
  result = TRUE;

exit:
  Py_XDECREF(py_args);
  return result;
}

static gboolean
_py_attach_get_headers(PythonHttpHeaderPlugin *self)
{
  self->py.get_headers = _py_get_attr_or_null(self->py.instance, "get_headers");
  if (!self->py.get_headers)
    {
      msg_error("Error initializing plugin, required method not found",
                evt_tag_str("class", self->class),
                evt_tag_str("method", "get_headers"));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_py_attach_on_http_response_received(PythonHttpHeaderPlugin *self)
{
  self->py.on_http_response_received = _py_get_attr_or_null(self->py.instance, "on_http_response_received");

  /*
   * on_http_response_received is an optional method
   * */
  return TRUE;
}

static gboolean
_py_attach_bindings(PythonHttpHeaderPlugin *self)
{
  return _py_attach_class(self) &&
         _py_instantiate_class(self) &&
         _py_attach_get_headers(self) &&
         _py_attach_on_http_response_received(self);
}

static void
_py_detach_bindings(PythonHttpHeaderPlugin *self)
{
  Py_CLEAR(self->py.class);
  Py_CLEAR(self->py.instance);
  Py_CLEAR(self->py.get_headers);
  Py_CLEAR(self->py.on_http_response_received);
}

static void
_append_str_to_list(gpointer data, gpointer user_data)
{
  List *list = (List *) user_data;
  list_append(list, data);
}

static HttpSlotResultType
_get_default_error_code(PythonHttpHeaderPlugin *self)
{
  if (self->mark_errors_as_critical)
    return HTTP_SLOT_CRITICAL_ERROR;

  return HTTP_SLOT_PLUGIN_ERROR;
}

static void
_append_headers(PythonHttpHeaderPlugin *self, HttpHeaderRequestSignalData *data)
{
  PyObject *py_list = NULL,
            *py_args = NULL,
             *py_ret_list = NULL;
  GList *headers = NULL;

  data->result = _get_default_error_code(self);

  // The GIL also guards non-Python plugin struct members from concurrent changes below.
  PyGILState_STATE gstate = PyGILState_Ensure();

  py_list = _py_convert_list_to_pylist(data->request_headers);
  if (!py_list)
    goto cleanup;

  py_args = Py_BuildValue("(sO)", data->request_body->str, py_list);
  if (!py_args)
    {
      gchar buf[256];
      _py_format_exception_text(buf, sizeof(buf));

      msg_error("Error creating Python arguments",
                evt_tag_str("class", self->class),
                evt_tag_str("exception", buf));
      _py_finish_exception_handling();

      goto cleanup;
    }

  py_ret_list = _py_invoke_function_with_args(self->py.get_headers, py_args, self->class, "_append_headers");
  if (!py_ret_list)
    {
      gchar buf[256];
      _py_format_exception_text(buf, sizeof(buf));

      msg_error("Invalid response returned by Python call",
                evt_tag_str("class", self->class),
                evt_tag_str("method", "get_headers"),
                evt_tag_str("exception", buf));
      _py_finish_exception_handling();

      goto cleanup;
    }

  msg_debug("Python call returned valid response",
            evt_tag_str("class", self->class),
            evt_tag_str("method", "get_headers"),
            evt_tag_str("return_type", py_ret_list->ob_type->tp_name));

  if (!_py_append_pylist_to_list(py_ret_list, &headers))
    {
      gchar buf[256];
      _py_format_exception_text(buf, sizeof(buf));

      msg_error("Converting Python List failed",
                evt_tag_str("class", self->class),
                evt_tag_str("method", "get_headers"),
                evt_tag_str("exception", buf));
      _py_finish_exception_handling();
      goto cleanup;
    }

  data->result = HTTP_SLOT_SUCCESS;

cleanup:
  Py_XDECREF(py_args);
  Py_XDECREF(py_list);
  Py_XDECREF(py_ret_list);

  PyGILState_Release(gstate);

  if (headers)
    {
      g_list_foreach(headers, _append_str_to_list, data->request_headers);
      g_list_free_full(headers, g_free);
    }
}

static void
_on_http_response_received(PythonHttpHeaderPlugin *self, HttpResponseReceivedSignalData *data)
{
  if (!self->py.on_http_response_received)
    return;

  PyGILState_STATE gstate = PyGILState_Ensure();
  {

    PyObject *py_arg = Py_BuildValue("i", data->http_code);
    if (!py_arg)
      {
        gchar buf[256];
        _py_format_exception_text(buf, sizeof(buf));

        msg_error("Error creating Python argument",
                  evt_tag_str("class", self->class),
                  evt_tag_str("exception", buf));
        _py_finish_exception_handling();
        return;
      }

    _py_invoke_void_function(self->py.on_http_response_received, py_arg, self->class, "_on_http_response_received");

    Py_XDECREF(py_arg);
  }
  PyGILState_Release(gstate);
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

static void
_connect_http_header_request_slot(LogDriverPlugin *s, SignalSlotConnector *ssc)
{
  PythonHttpHeaderPlugin *self = (PythonHttpHeaderPlugin *) s;
  CONNECT(ssc, signal_http_header_request, _append_headers, self);

  msg_debug("SignalSlotConnector slot registered",
            evt_tag_printf("connector", "%p", ssc),
            evt_tag_printf("signal", "%s", signal_http_header_request),
            evt_tag_printf("plugin_name", "%s", PYTHON_HTTP_HEADER_PLUGIN),
            evt_tag_printf("plugin_instance", "%p", s));
}

static void
_connect_http_response_received_slot(LogDriverPlugin *s, SignalSlotConnector *ssc)
{
  PythonHttpHeaderPlugin *self = (PythonHttpHeaderPlugin *) s;
  CONNECT(ssc, signal_http_response_received, _on_http_response_received, self);

  msg_debug("SignalSlotConnector slot registered",
            evt_tag_printf("connector", "%p", ssc),
            evt_tag_printf("signal", "%s", signal_http_response_received),
            evt_tag_printf("plugin_name", "%s", PYTHON_HTTP_HEADER_PLUGIN),
            evt_tag_printf("plugin_instance", "%p", s));
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
  _connect_http_header_request_slot(s, ssc);
  _connect_http_response_received_slot(s, ssc);

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

  self->mark_errors_as_critical = TRUE;
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

void
python_http_header_set_mark_errors_as_critical(PythonHttpHeaderPlugin *self, gboolean enable)
{
  self->mark_errors_as_critical = enable;
}
