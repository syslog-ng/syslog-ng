/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 László Várady <laszlo.varady@balabit.com>
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

#include "python-fetcher.h"
#include "python-logmsg.h"
#include "python-helpers.h"
#include "logthrsource/logthrfetcherdrv.h"
#include "str-utils.h"
#include "string-list.h"
#include "python-persist.h"

#include <structmember.h>

typedef struct _PythonFetcherDriver
{
  LogThreadedFetcherDriver super;

  gchar *class;
  GList *loaders;
  GHashTable *options;

  struct
  {
    PyObject *class;
    PyObject *instance;
    PyObject *fetch_method;
    PyObject *open_method;
    PyObject *close_method;
    PyObject *request_exit_method;
    PyObject *generate_persist_name;
  } py;
} PythonFetcherDriver;

typedef struct _PyLogFetcher
{
  PyObject_HEAD
  PythonFetcherDriver *driver;
  gchar *persist_name;
} PyLogFetcher;

static PyTypeObject py_log_fetcher_type;


void
python_fetcher_set_class(LogDriver *s, gchar *filename)
{
  PythonFetcherDriver *self = (PythonFetcherDriver *) s;

  g_free(self->class);
  self->class = g_strdup(filename);
}

void
python_fetcher_set_option(LogDriver *s, gchar *key, gchar *value)
{
  PythonFetcherDriver *self = (PythonFetcherDriver *) s;
  gchar *normalized_key = __normalize_key(key);
  g_hash_table_insert(self->options, normalized_key, g_strdup(value));
}

void
python_fetcher_set_loaders(LogDriver *s, GList *loaders)
{
  PythonFetcherDriver *self = (PythonFetcherDriver *) s;

  string_list_free(self->loaders);
  self->loaders = loaders;
}

static const gchar *
python_fetcher_format_stats_instance(LogThreadedSourceDriver *s)
{
  PythonFetcherDriver *self = (PythonFetcherDriver *) s;
  return python_format_stats_instance((LogPipe *)s, self->py.generate_persist_name, self->options,
                                      "python-fetcher", self->class);
}

static void
_pf_py_invoke_void_method_by_name(PythonFetcherDriver *self, const gchar *method_name)
{
  _py_invoke_void_method_by_name(self->py.instance, method_name, self->class, self->super.super.super.super.id);
}

static gboolean
_pf_py_invoke_bool_method_by_name_with_args(PythonFetcherDriver *self, const gchar *method_name)
{
  return _py_invoke_bool_method_by_name_with_args(self->py.instance, method_name, self->options, self->class,
                                                  self->super.super.super.super.id);
}

static void
_pf_py_invoke_void_function(PythonFetcherDriver *self, PyObject *func, PyObject *arg)
{
  return _py_invoke_void_function(func, arg, self->class, self->super.super.super.super.id);
}

static gboolean
_pf_py_invoke_bool_function(PythonFetcherDriver *self, PyObject *func, PyObject *arg)
{
  return _py_invoke_bool_function(func, arg, self->class, self->super.super.super.super.id);
}

static gboolean
_py_invoke_init(PythonFetcherDriver *self)
{
  return _pf_py_invoke_bool_method_by_name_with_args(self, "init");
}

static void
_py_invoke_deinit(PythonFetcherDriver *self)
{
  _pf_py_invoke_void_method_by_name(self, "deinit");
}

static void
_py_invoke_request_exit(PythonFetcherDriver *self)
{
  _pf_py_invoke_void_function(self, self->py.request_exit_method, NULL);
}

static gboolean
_py_invoke_open(PythonFetcherDriver *self)
{
  return _pf_py_invoke_bool_function(self, self->py.open_method, NULL);
}

static void
_py_invoke_close(PythonFetcherDriver *self)
{
  _pf_py_invoke_void_function(self, self->py.close_method, NULL);
}

static inline gboolean
_ulong_to_fetch_result(unsigned long ulong, ThreadedFetchResult *result)
{
  switch (ulong)
    {
    case THREADED_FETCH_ERROR:
    case THREADED_FETCH_NOT_CONNECTED:
    case THREADED_FETCH_SUCCESS:
    case THREADED_FETCH_TRY_AGAIN:
    case THREADED_FETCH_NO_DATA:
      *result = (ThreadedFetchResult) ulong;
      return TRUE;

    default:
      return FALSE;
    }
}

static ThreadedFetchResult
_py_invoke_fetch(PythonFetcherDriver *self, LogMessage **msg)
{
  PyObject *ret = _py_invoke_function(self->py.fetch_method, NULL, self->class, self->super.super.super.super.id);

  if (!ret || !PyTuple_Check(ret) || PyTuple_Size(ret) > 2)
    goto error;

  PyObject *result = PyTuple_GetItem(ret, 0);
  if (!result || !PyLong_Check(result))
    goto error;

  ThreadedFetchResult fetch_result;
  if (!_ulong_to_fetch_result(PyLong_AsUnsignedLong(result), &fetch_result))
    goto error;

  if (fetch_result == THREADED_FETCH_SUCCESS)
    {
      PyLogMessage *pymsg = (PyLogMessage *) PyTuple_GetItem(ret, 1);
      if (!pymsg || !py_is_log_message((PyObject *) pymsg))
        goto error;

      /* keep a reference until the PyLogMessage instance is freed */
      *msg = log_msg_ref(pymsg->msg);
    }

  Py_XDECREF(ret);
  PyErr_Clear();
  return fetch_result;

error:
  msg_error("Error in Python fetcher, fetch() must return a tuple (FetchResult, LogMessage)",
            evt_tag_str("driver", self->super.super.super.super.id),
            evt_tag_str("class", self->class));

  Py_XDECREF(ret);
  PyErr_Clear();

  return THREADED_FETCH_ERROR;
}

static gboolean
_py_is_log_fetcher(PyObject *obj)
{
  return PyType_IsSubtype(Py_TYPE(obj), &py_log_fetcher_type);
}

static void
_py_free_bindings(PythonFetcherDriver *self)
{
  PyLogFetcher *py_instance = (PyLogFetcher *) self->py.instance;
  if (py_instance)
    g_free(py_instance->persist_name);

  Py_CLEAR(self->py.class);
  Py_CLEAR(self->py.instance);
  Py_CLEAR(self->py.fetch_method);
  Py_CLEAR(self->py.open_method);
  Py_CLEAR(self->py.close_method);
  Py_CLEAR(self->py.request_exit_method);
  Py_CLEAR(self->py.generate_persist_name);
}

static gboolean
_py_resolve_class(PythonFetcherDriver *self)
{
  self->py.class = _py_resolve_qualified_name(self->class);

  if (!self->py.class)
    {
      gchar buf[256];

      msg_error("Error looking Python driver class",
                evt_tag_str("driver", self->super.super.super.super.id),
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }

  return TRUE;
}

static gboolean
_py_init_instance(PythonFetcherDriver *self)
{
  self->py.instance = _py_invoke_function(self->py.class, NULL, self->class, self->super.super.super.super.id);

  if (!self->py.instance)
    {
      gchar buf[256];

      msg_error("Error instantiating Python driver class",
                evt_tag_str("driver", self->super.super.super.super.id),
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }

  if (!_py_is_log_fetcher(self->py.instance))
    {
      msg_error("Error initializing Python fetcher, class is not a subclass of LogFetcher",
                evt_tag_str("driver", self->super.super.super.super.id),
                evt_tag_str("class", self->class));
      return FALSE;
    }

  ((PyLogFetcher *) self->py.instance)->driver = self;

  return TRUE;
}

static gboolean
_py_lookup_fetch_method(PythonFetcherDriver *self)
{
  self->py.fetch_method = _py_get_attr_or_null(self->py.instance, "fetch");

  if (!self->py.fetch_method)
    {
      msg_error("Error initializing Python fetcher, class does not have a fetch() method",
                evt_tag_str("driver", self->super.super.super.super.id),
                evt_tag_str("class", self->class));
      return FALSE;
    }

  return TRUE;
}

static const gchar *python_fetcher_format_persist_name(const LogPipe *s);
static void
_py_set_persist_name(PythonFetcherDriver *self)
{
  const gchar *persist_name = python_fetcher_format_persist_name((LogPipe *)self);
  PyLogFetcher *py_instance = (PyLogFetcher *) self->py.instance;
  py_instance->persist_name = g_strdup(persist_name);
}

static gboolean
_py_init_methods(PythonFetcherDriver *self)
{
  if (!_py_lookup_fetch_method(self))
    return FALSE;

  self->py.request_exit_method = _py_get_attr_or_null(self->py.instance, "request_exit");
  self->py.open_method = _py_get_attr_or_null(self->py.instance, "open");
  self->py.close_method = _py_get_attr_or_null(self->py.instance, "close");
  self->py.generate_persist_name = _py_get_attr_or_null(self->py.instance, "generate_persist_name");

  _py_set_persist_name(self);

  return TRUE;
}

static gboolean
_py_init_bindings(PythonFetcherDriver *self)
{
  gboolean initialized = _py_resolve_class(self)
                         && _py_init_instance(self)
                         && _py_init_methods(self);

  if (!initialized)
    _py_free_bindings(self);

  return initialized;
}

static gboolean
_py_init_object(PythonFetcherDriver *self)
{
  if (!_py_get_attr_or_null(self->py.instance, "init"))
    {
      msg_debug("Missing Python method, init()",
                evt_tag_str("driver", self->super.super.super.super.id),
                evt_tag_str("class", self->class));
      return TRUE;
    }

  if (!_py_invoke_init(self))
    {
      msg_error("Error initializing Python driver object, init() returned FALSE",
                evt_tag_str("driver", self->super.super.super.super.id),
                evt_tag_str("class", self->class));
      return FALSE;
    }
  return TRUE;
}

static PyObject *
_py_parse_options_new(PythonFetcherDriver *self, MsgFormatOptions *parse_options)
{
  PyObject *py_parse_options = PyCapsule_New(parse_options, NULL, NULL);

  if (!py_parse_options)
    {
      gchar buf[256];

      msg_error("Error creating capsule for message parse options",
                evt_tag_str("driver", self->super.super.super.super.id),
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return NULL;
    }

  return py_parse_options;
}

static gboolean
_py_set_parse_options(PythonFetcherDriver *self)
{
  MsgFormatOptions *parse_options = log_threaded_source_driver_get_parse_options(&self->super.super.super.super);

  PyObject *py_parse_options = _py_parse_options_new(self, parse_options);
  if (!py_parse_options)
    return FALSE;

  if (PyObject_SetAttrString(self->py.instance, "parse_options", py_parse_options) == -1)
    {
      gchar buf[256];

      msg_error("Error setting attribute message parse options",
                evt_tag_str("driver", self->super.super.super.super.id),
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();

      Py_DECREF(py_parse_options);
      return FALSE;
    }

  Py_DECREF(py_parse_options);
  return TRUE;
}

static gboolean
python_fetcher_open(LogThreadedFetcherDriver *s)
{
  PythonFetcherDriver *self = (PythonFetcherDriver *) s;

  PyGILState_STATE gstate = PyGILState_Ensure();
  gboolean result = _py_invoke_open(self);
  PyGILState_Release(gstate);

  return result;
}

static void
python_fetcher_close(LogThreadedFetcherDriver *s)
{
  PythonFetcherDriver *self = (PythonFetcherDriver *) s;

  PyGILState_STATE gstate = PyGILState_Ensure();
  _py_invoke_close(self);
  PyGILState_Release(gstate);
}

static void
python_fetcher_request_exit(LogThreadedFetcherDriver *s)
{
  PythonFetcherDriver *self = (PythonFetcherDriver *) s;

  PyGILState_STATE gstate = PyGILState_Ensure();
  _py_invoke_request_exit(self);
  PyGILState_Release(gstate);
}

static gboolean
_py_fetcher_init(PythonFetcherDriver *self)
{
  PyGILState_STATE gstate = PyGILState_Ensure();

  _py_perform_imports(self->loaders);
  if (!_py_init_bindings(self))
    goto error;

  if (self->py.open_method)
    self->super.connect = python_fetcher_open;

  if (self->py.close_method)
    self->super.disconnect = python_fetcher_close;

  if (self->py.request_exit_method)
    self->super.request_exit = python_fetcher_request_exit;

  if (!_py_init_object(self))
    goto error;

  if (!_py_set_parse_options(self))
    goto error;

  PyGILState_Release(gstate);
  return TRUE;

error:
  PyGILState_Release(gstate);
  return FALSE;
}

static LogThreadedFetchResult
python_fetcher_fetch(LogThreadedFetcherDriver *s)
{
  PythonFetcherDriver *self = (PythonFetcherDriver *) s;
  LogThreadedFetchResult fetch_result;

  PyGILState_STATE gstate = PyGILState_Ensure();
  {
    LogMessage *msg = NULL;
    ThreadedFetchResult result = _py_invoke_fetch(self, &msg);

    fetch_result = (LogThreadedFetchResult)
    {
      result, msg
    };
  }
  PyGILState_Release(gstate);

  return fetch_result;
}

static const gchar *
python_fetcher_format_persist_name(const LogPipe *s)
{
  const PythonFetcherDriver *self = (const PythonFetcherDriver *)s;
  return python_format_persist_name(s, self->py.generate_persist_name, self->options, "python-fetcher", self->class);
}

static gboolean
python_fetcher_init(LogPipe *s)
{
  PythonFetcherDriver *self = (PythonFetcherDriver *) s;

  if (!self->class)
    {
      msg_error("Error initializing Python fetcher: no script specified!",
                evt_tag_str("driver", self->super.super.super.super.id));
      return FALSE;
    }

  self->super.time_reopen = 1;

  if (!_py_fetcher_init(self))
    return FALSE;

  msg_verbose("Python fetcher initialized",
              evt_tag_str("driver", self->super.super.super.super.id),
              evt_tag_str("class", self->class));

  return log_threaded_fetcher_driver_init_method(s);
}

static gboolean
python_fetcher_deinit(LogPipe *s)
{
  PythonFetcherDriver *self = (PythonFetcherDriver *) s;

  PyGILState_STATE gstate = PyGILState_Ensure();
  _py_invoke_deinit(self);
  PyGILState_Release(gstate);

  return log_threaded_fetcher_driver_deinit_method(s);
}

static void
python_fetcher_free(LogPipe *s)
{
  PythonFetcherDriver *self = (PythonFetcherDriver *) s;

  PyGILState_STATE gstate = PyGILState_Ensure();
  _py_free_bindings(self);
  PyGILState_Release(gstate);

  g_free(self->class);
  g_hash_table_unref(self->options);
  string_list_free(self->loaders);

  log_threaded_fetcher_driver_free_method(s);
}

LogDriver *
python_fetcher_new(GlobalConfig *cfg)
{
  PythonFetcherDriver *self = g_new0(PythonFetcherDriver, 1);

  log_threaded_fetcher_driver_init_instance(&self->super, cfg);
  self->super.super.super.super.super.init = python_fetcher_init;
  self->super.super.super.super.super.deinit = python_fetcher_deinit;
  self->super.super.super.super.super.free_fn = python_fetcher_free;
  self->super.super.super.super.super.generate_persist_name = python_fetcher_format_persist_name;

  self->super.super.format_stats_instance = python_fetcher_format_stats_instance;
  self->super.super.worker_options.super.stats_level = STATS_LEVEL0;
  self->super.super.worker_options.super.stats_source = stats_register_type("python");

  self->super.fetch = python_fetcher_fetch;

  self->options = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  return &self->super.super.super.super;
}

static PyMemberDef py_log_fetcher_members[] =
{
  { "persist_name", T_STRING, offsetof(PyLogFetcher, persist_name), READONLY },
  {NULL}
};

static PyTypeObject py_log_fetcher_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "LogFetcher",
  .tp_basicsize = sizeof(PyLogFetcher),
  .tp_dealloc = py_slng_generic_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "The LogFetcher class is a base class for custom Python fetchers.",
  .tp_new = PyType_GenericNew,
  .tp_members = py_log_fetcher_members,
  0,
};

void
py_log_fetcher_init(void)
{
  py_log_fetcher_type.tp_dict = PyDict_New();
  PyDict_SetItemString(py_log_fetcher_type.tp_dict, "FETCH_ERROR",
                       PyLong_FromLong(THREADED_FETCH_ERROR));
  PyDict_SetItemString(py_log_fetcher_type.tp_dict, "FETCH_NOT_CONNECTED",
                       PyLong_FromLong(THREADED_FETCH_NOT_CONNECTED));
  PyDict_SetItemString(py_log_fetcher_type.tp_dict, "FETCH_SUCCESS",
                       PyLong_FromLong(THREADED_FETCH_SUCCESS));
  PyDict_SetItemString(py_log_fetcher_type.tp_dict, "FETCH_TRY_AGAIN",
                       PyLong_FromLong(THREADED_FETCH_TRY_AGAIN));
  PyDict_SetItemString(py_log_fetcher_type.tp_dict, "FETCH_NO_DATA",
                       PyLong_FromLong(THREADED_FETCH_NO_DATA));

  PyType_Ready(&py_log_fetcher_type);
  PyModule_AddObject(PyImport_AddModule("_syslogng"), "LogFetcher", (PyObject *) &py_log_fetcher_type);
}
