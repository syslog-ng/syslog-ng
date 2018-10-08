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

#include "python-source.h"
#include "python-logmsg.h"
#include "python-helpers.h"
#include "logthrsource/logthrsourcedrv.h"
#include "str-utils.h"
#include "string-list.h"

typedef struct _PythonSourceDriver PythonSourceDriver;

struct _PythonSourceDriver
{
  LogThreadedSourceDriver super;

  gchar *class;
  GList *loaders;
  GHashTable *options;

  void (*post_message)(PythonSourceDriver *self, LogMessage *msg);

  struct
  {
    PyObject *class;
    PyObject *instance;
    PyObject *run_method;
    PyObject *request_exit_method;
    PyObject *suspend_method;
    PyObject *wakeup_method;
  } py;
};

typedef struct _PyLogSource
{
  PyObject_HEAD
  PythonSourceDriver *driver;
} PyLogSource;

static PyTypeObject py_log_source_type;


void
python_sd_set_class(LogDriver *s, gchar *filename)
{
  PythonSourceDriver *self = (PythonSourceDriver *) s;

  g_free(self->class);
  self->class = g_strdup(filename);
}

void
python_sd_set_option(LogDriver *s, gchar *key, gchar *value)
{
  PythonSourceDriver *self = (PythonSourceDriver *) s;
  gchar *normalized_key = __normalize_key(key);
  g_hash_table_insert(self->options, normalized_key, g_strdup(value));
}

void
python_sd_set_loaders(LogDriver *s, GList *loaders)
{
  PythonSourceDriver *self = (PythonSourceDriver *) s;

  string_list_free(self->loaders);
  self->loaders = loaders;
}

static const gchar *
python_sd_format_stats_instance(LogThreadedSourceDriver *s)
{
  PythonSourceDriver *self = (PythonSourceDriver *) s;
  static gchar persist_name[1024];

  if (s->super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "python,%s", s->super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "python,%s", self->class);

  return persist_name;
}

static void
_ps_py_invoke_void_method_by_name(PythonSourceDriver *self, const gchar *method_name)
{
  _py_invoke_void_method_by_name(self->py.instance, method_name, self->class, self->super.super.super.id);
}

static gboolean
_ps_py_invoke_bool_method_by_name_with_args(PythonSourceDriver *self, const gchar *method_name)
{
  return _py_invoke_bool_method_by_name_with_args(self->py.instance, method_name, self->options, self->class,
                                                  self->super.super.super.id);
}

static void
_ps_py_invoke_void_function(PythonSourceDriver *self, PyObject *func, PyObject *arg)
{
  return _py_invoke_void_function(func, arg, self->class, self->super.super.super.id);
}

static gboolean
_py_invoke_init(PythonSourceDriver *self)
{
  return _ps_py_invoke_bool_method_by_name_with_args(self, "init");
}

static void
_py_invoke_deinit(PythonSourceDriver *self)
{
  _ps_py_invoke_void_method_by_name(self, "deinit");
}

static void
_py_invoke_run(PythonSourceDriver *self)
{
  _ps_py_invoke_void_function(self, self->py.run_method, NULL);
}

static void
_py_invoke_request_exit(PythonSourceDriver *self)
{
  _ps_py_invoke_void_function(self, self->py.request_exit_method, NULL);
}

static void
_py_invoke_suspend(PythonSourceDriver *self)
{
  _ps_py_invoke_void_function(self, self->py.suspend_method, NULL);
}

static void
_py_invoke_wakeup(PythonSourceDriver *self)
{
  _ps_py_invoke_void_function(self, self->py.wakeup_method, NULL);
}

static gboolean
_py_is_log_source(PyObject *obj)
{
  return PyType_IsSubtype(Py_TYPE(obj), &py_log_source_type);
}

static void
_py_free_bindings(PythonSourceDriver *self)
{
  Py_CLEAR(self->py.class);
  Py_CLEAR(self->py.instance);
  Py_CLEAR(self->py.run_method);
  Py_CLEAR(self->py.request_exit_method);
  Py_CLEAR(self->py.suspend_method);
  Py_CLEAR(self->py.wakeup_method);
}

static gboolean
_py_resolve_class(PythonSourceDriver *self)
{
  self->py.class = _py_resolve_qualified_name(self->class);

  if (!self->py.class)
    {
      gchar buf[256];

      msg_error("Error looking Python driver class",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }

  return TRUE;
}

static gboolean
_py_init_instance(PythonSourceDriver *self)
{
  self->py.instance = _py_invoke_function(self->py.class, NULL, self->class, self->super.super.super.id);

  if (!self->py.instance)
    {
      gchar buf[256];

      msg_error("Error instantiating Python driver class",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }

  if (!_py_is_log_source(self->py.instance))
    {
      msg_error("Error initializing Python source, class is not a subclass of LogSource",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->class));
      return FALSE;
    }

  ((PyLogSource *) self->py.instance)->driver = self;

  return TRUE;
}

static gboolean
_py_lookup_run_method(PythonSourceDriver *self)
{
  self->py.run_method = _py_get_attr_or_null(self->py.instance, "run");

  if (!self->py.run_method)
    {
      msg_error("Error initializing Python source, class does not have a run() method",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->class));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_py_lookup_request_exit_method(PythonSourceDriver *self)
{
  self->py.request_exit_method = _py_get_attr_or_null(self->py.instance, "request_exit");

  if (!self->py.request_exit_method)
    {
      msg_error("Error initializing Python source, class does not have a request_exit() method",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->class));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_py_lookup_suspend_and_wakeup_methods(PythonSourceDriver *self)
{
  self->py.suspend_method = _py_get_attr_or_null(self->py.instance, "suspend");

  if (self->py.suspend_method)
    {
      self->py.wakeup_method = _py_get_attr_or_null(self->py.instance, "wakeup");
      if (!self->py.wakeup_method)
        {
          msg_error("Error initializing Python source, class implements suspend() but wakeup() is missing",
                    evt_tag_str("driver", self->super.super.super.id),
                    evt_tag_str("class", self->class));
          return FALSE;
        }
    }

  return TRUE;
}

static gboolean
_py_init_methods(PythonSourceDriver *self)
{
  return _py_lookup_run_method(self)
         && _py_lookup_request_exit_method(self)
         && _py_lookup_suspend_and_wakeup_methods(self);
}

static gboolean
_py_init_bindings(PythonSourceDriver *self)
{
  gboolean initialized = _py_resolve_class(self)
                         && _py_init_instance(self)
                         && _py_init_methods(self);

  if (!initialized)
    _py_free_bindings(self);

  return initialized;
}

static gboolean
_py_init_object(PythonSourceDriver *self)
{
  if (!_py_get_attr_or_null(self->py.instance, "init"))
    {
      msg_debug("Missing Python method, init()",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->class));
      return TRUE;
    }

  if (!_py_invoke_init(self))
    {
      msg_error("Error initializing Python driver object, init() returned FALSE",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->class));
      return FALSE;
    }
  return TRUE;
}

static gboolean
_py_set_parse_options(PythonSourceDriver *self)
{
  MsgFormatOptions *parse_options = log_threaded_source_driver_get_parse_options(&self->super.super.super);

  PyObject *py_parse_options = PyCapsule_New(parse_options, NULL, NULL);

  if (!py_parse_options)
    {
      gchar buf[256];

      msg_error("Error creating capsule for message parse options",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }

  if (PyObject_SetAttrString(self->py.instance, "parse_options", py_parse_options) == -1)
    {
      gchar buf[256];

      msg_error("Error setting attribute message parse options",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();

      Py_DECREF(py_parse_options);
      return FALSE;
    }

  Py_DECREF(py_parse_options);
  return TRUE;
}

static void
python_sd_suspend(PythonSourceDriver *self)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  _py_invoke_suspend(self);
  PyGILState_Release(gstate);
}

static void
python_sd_wakeup(LogThreadedSourceDriver *s)
{
  PythonSourceDriver *self = (PythonSourceDriver *) s;

  PyGILState_STATE gstate = PyGILState_Ensure();
  _py_invoke_wakeup(self);
  PyGILState_Release(gstate);
}

static void
_post_message_non_blocking(PythonSourceDriver *self, LogMessage *msg)
{
  PyThreadState *state = PyEval_SaveThread();
  log_threaded_source_post(&self->super, msg);
  PyEval_RestoreThread(state);

  /* GIL is used to synchronize free_to_send(), suspend() and wakeup() */
  if (!log_threaded_source_free_to_send(&self->super))
    python_sd_suspend(self);
}

static void
_post_message_blocking(PythonSourceDriver *self, LogMessage *msg)
{
  PyThreadState *state = PyEval_SaveThread();
  log_threaded_source_blocking_post(&self->super, msg);
  PyEval_RestoreThread(state);
}

static gboolean
_py_sd_init(PythonSourceDriver *self)
{
  PyGILState_STATE gstate = PyGILState_Ensure();

  _py_perform_imports(self->loaders);
  if (!_py_init_bindings(self))
    goto error;

  if (self->py.suspend_method && self->py.wakeup_method)
    {
      self->post_message = _post_message_non_blocking;
      log_threaded_source_set_wakeup_func(&self->super, python_sd_wakeup);
    }

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

static PyObject *
py_log_source_post(PyObject *s, PyObject *args, PyObject *kwrds)
{
  PyLogSource *self = (PyLogSource *) s;
  PythonSourceDriver *sd = self->driver;

  PyLogMessage *pymsg;

  static const gchar *kwlist[] = {"msg", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwrds, "O", (gchar **) kwlist, &pymsg))
    return NULL;

  if (!py_is_log_message((PyObject *) pymsg))
    {
      PyErr_Format(PyExc_TypeError, "LogMessage expected in the first parameter");
      return NULL;
    }

  if (!log_threaded_source_free_to_send(&sd->super))
    {
      msg_error("Incorrectly suspended source, dropping message",
                evt_tag_str("driver", sd->super.super.super.id));
      Py_RETURN_NONE;
    }

  /* keep a reference until the PyLogMessage instance is freed */
  LogMessage *message = log_msg_ref(pymsg->msg);
  sd->post_message(sd, message);

  Py_RETURN_NONE;
}

static void
python_sd_run(LogThreadedSourceDriver *s)
{
  PythonSourceDriver *self = (PythonSourceDriver *) s;

  PyGILState_STATE gstate = PyGILState_Ensure();
  _py_invoke_run(self);
  PyGILState_Release(gstate);
}

static void
python_sd_request_exit(LogThreadedSourceDriver *s)
{
  PythonSourceDriver *self = (PythonSourceDriver *) s;

  PyGILState_STATE gstate = PyGILState_Ensure();
  _py_invoke_request_exit(self);
  PyGILState_Release(gstate);
}

static gboolean
python_sd_init(LogPipe *s)
{
  PythonSourceDriver *self = (PythonSourceDriver *) s;

  if (!self->class)
    {
      msg_error("Error initializing Python source: no script specified!",
                evt_tag_str("driver", self->super.super.super.id));
      return FALSE;
    }

  if(!_py_sd_init(self))
    return FALSE;

  msg_verbose("Python source initialized",
              evt_tag_str("driver", self->super.super.super.id),
              evt_tag_str("class", self->class));

  return log_threaded_source_driver_init_method(s);
}

static gboolean
python_sd_deinit(LogPipe *s)
{
  PythonSourceDriver *self = (PythonSourceDriver *) s;

  PyGILState_STATE gstate = PyGILState_Ensure();
  _py_invoke_deinit(self);
  PyGILState_Release(gstate);

  return log_threaded_source_driver_deinit_method(s);
}

static void
python_sd_free(LogPipe *s)
{
  PythonSourceDriver *self = (PythonSourceDriver *) s;

  PyGILState_STATE gstate = PyGILState_Ensure();
  _py_free_bindings(self);
  PyGILState_Release(gstate);

  g_free(self->class);
  g_hash_table_unref(self->options);
  string_list_free(self->loaders);

  log_threaded_source_driver_free_method(s);
}

LogDriver *
python_sd_new(GlobalConfig *cfg)
{
  PythonSourceDriver *self = g_new0(PythonSourceDriver, 1);

  log_threaded_source_driver_init_instance(&self->super, cfg);
  self->super.super.super.super.init = python_sd_init;
  self->super.super.super.super.deinit = python_sd_deinit;
  self->super.super.super.super.free_fn = python_sd_free;

  self->super.format_stats_instance = python_sd_format_stats_instance;
  self->super.worker_options.super.stats_level = STATS_LEVEL0;
  self->super.worker_options.super.stats_source = SCS_PYTHON;

  log_threaded_source_driver_set_worker_request_exit_func(&self->super, python_sd_request_exit);
  log_threaded_source_driver_set_worker_run_func(&self->super, python_sd_run);

  self->options = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  self->post_message = _post_message_blocking;

  return &self->super.super.super;
}


static PyMethodDef py_log_source_methods[] =
{
  { "post_message", (PyCFunction) py_log_source_post, METH_VARARGS | METH_KEYWORDS, "Post message" },
  {NULL}
};

static PyTypeObject py_log_source_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "LogSource",
  .tp_basicsize = sizeof(PyLogSource),
  .tp_dealloc = (destructor) PyObject_Del,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "The LogSource class is a base class for custom Python sources.",
  .tp_new = PyType_GenericNew,
  .tp_methods = py_log_source_methods,
  0,
};

void
py_log_source_init(void)
{
  PyType_Ready(&py_log_source_type);
  PyModule_AddObject(PyImport_AddModule("syslogng"), "LogSource", (PyObject *) &py_log_source_type);
}
