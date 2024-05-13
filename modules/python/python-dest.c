/*
 * Copyright (c) 2014-2015 Balabit
 * Copyright (c) 2014 Gergely Nagy <algernon@balabit.hu>
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
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

#include "python-dest.h"
#include "python-module.h"
#include "python-value-pairs.h"
#include "python-logmsg.h"
#include "python-logtemplate-options.h"
#include "python-integerpointer.h"
#include "python-helpers.h"
#include "python-types.h"
#include "logthrdest/logthrdestdrv.h"
#include "stats/stats.h"
#include "string-list.h"
#include "str-utils.h"
#include "messages.h"
#include "python-persist.h"

typedef struct
{
  LogThreadedDestDriver super;

  PythonBinding binding;

  LogTemplateOptions template_options;
  ValuePairs *vp;

  struct
  {
    PyObject *class;
    PyObject *instance;
    PyObject *is_opened;
    PyObject *open;
    PyObject *send;
    PyObject *flush;
    PyObject *generate_persist_name;
    GPtrArray *_refs_to_clean;
  } py;
} PythonDestDriver;

typedef struct _PyLogDestination
{
  PyObject_HEAD
  PythonDestDriver *driver;
} PyLogDestination;

static PyTypeObject py_log_destination_type;

static gboolean
_py_is_log_destination(PyObject *obj)
{
  return PyType_IsSubtype(Py_TYPE(obj), &py_log_destination_type);
}

/** Setters & config glue **/

PythonBinding *
python_dd_get_binding(LogDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  return &self->binding;
}

void
python_dd_set_value_pairs(LogDriver *d, ValuePairs *vp)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  value_pairs_unref(self->vp);
  self->vp = vp;
}

LogTemplateOptions *
python_dd_get_template_options(LogDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  return &self->template_options;
}

/** Helpers for stats & persist_name formatting **/

static const gchar *
python_dd_format_stats_key(LogThreadedDestDriver *d, StatsClusterKeyBuilder *kb)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  PythonPersistMembers options =
  {
    .generate_persist_name_method = self->py.generate_persist_name,
    .options = self->binding.options,
    .class = self->binding.class,
    .id = self->super.super.super.id
  };

  return python_format_stats_key((LogPipe *)d, kb, "python", &options);
}

static const gchar *
python_dd_format_persist_name(const LogPipe *s)
{
  const PythonDestDriver *self = (const PythonDestDriver *)s;

  PythonPersistMembers options =
  {
    .generate_persist_name_method = self->py.generate_persist_name,
    .options = self->binding.options,
    .class = self->binding.class,
    .id = self->super.super.super.id
  };

  return python_format_persist_name(s, "python", &options);
}

static gboolean
_dd_py_invoke_bool_function(PythonDestDriver *self, PyObject *func, PyObject *arg)
{
  return _py_invoke_bool_function(func, arg, self->binding.class, self->super.super.super.id);
}

static void
_dd_py_invoke_void_method_by_name(PythonDestDriver *self, const gchar *method_name)
{
  _py_invoke_void_method_by_name(self->py.instance, method_name, self->binding.class, self->super.super.super.id);
}

static gboolean
_dd_py_invoke_bool_method_by_name_with_options(PythonDestDriver *self, const gchar *method_name)
{
  return _py_invoke_bool_method_by_name_with_options(self->py.instance, method_name, self->binding.options,
                                                     self->binding.class, self->super.super.super.id);
}

static gboolean
_py_invoke_is_opened(PythonDestDriver *self)
{
  if (!self->py.is_opened)
    return TRUE;

  return _dd_py_invoke_bool_function(self, self->py.is_opened, NULL);
}

static gboolean
_py_invoke_open(PythonDestDriver *self)
{
  if (!self->py.open)
    return TRUE;

  PyObject *ret;
  gboolean result = FALSE;

  ret = _py_invoke_function(self->py.open, NULL, self->binding.class, self->super.super.super.id);
  if (ret)
    {
      if (ret == Py_None)
        {
          msg_warning_once("python-dest: Since " VERSION_3_25 ", the return value of the open() method "
                           "is used as success/failure indicator. Please use return True or return False explicitly",
                           evt_tag_str("class", self->binding.class));
          result = TRUE;
        }
      else
        result = PyObject_IsTrue(ret);
    }
  Py_XDECREF(ret);

  if (self->py.is_opened)
    {
      if (!result)
        return FALSE;

      return _py_invoke_is_opened(self);
    }

  return result;
}

static void
_py_invoke_close(PythonDestDriver *self)
{
  _dd_py_invoke_void_method_by_name(self, "close");
}

static LogThreadedResult
_as_int(PyObject *obj)
{
  gint64 result;
  if (!py_long_to_long(obj, &result) && PyErr_Occurred())
    {
      gchar buf[256];

      msg_error("python-dest: Error converting the result of send() to a LogDestinationResult enum. Retrying message later",
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return LTR_ERROR;
    }

  if (result < 0 || result >= LTR_MAX)
    {
      msg_error("python-dest: The result of send() is out of range, please use the LogDestinationResult enum (or a bool) as return value. Retrying message later",
                evt_tag_int("result", result));
      return LTR_ERROR;
    }

  return result;
}

static LogThreadedResult
_as_bool(PyObject *obj)
{
  return PyObject_IsTrue(obj) ? LTR_SUCCESS : LTR_ERROR;
}

static LogThreadedResult
pyobject_to_worker_insert_result(PyObject *obj)
{
  if (PyBool_Check(obj))
    return _as_bool(obj);
  else
    return _as_int(obj);
}

static LogThreadedResult
_py_invoke_flush(PythonDestDriver *self)
{
  if (!self->py.flush)
    return LTR_SUCCESS;

  PyObject *ret = _py_invoke_function(self->py.flush, NULL, self->binding.class, self->super.super.super.id);
  if (!ret)
    return LTR_ERROR;

  LogThreadedResult result = pyobject_to_worker_insert_result(ret);
  Py_XDECREF(ret);
  return result;
}

static LogThreadedResult
_py_invoke_send(PythonDestDriver *self, PyObject *dict)
{
  PyObject *ret;
  ret = _py_invoke_function(self->py.send, dict, self->binding.class, self->super.super.super.id);

  if (!ret)
    return LTR_ERROR;

  LogThreadedResult result = pyobject_to_worker_insert_result(ret);
  Py_XDECREF(ret);
  return result;
}

static gboolean
_py_invoke_init(PythonDestDriver *self)
{
  return _dd_py_invoke_bool_method_by_name_with_options(self, "init");
}

static void
_py_invoke_deinit(PythonDestDriver *self)
{
  _dd_py_invoke_void_method_by_name(self, "deinit");
}

static void
_py_clear(PyObject *self)
{
  Py_CLEAR(self);
}

#define _inject_worker_insert_result(self, value) \
  _inject_const(self, #value, LTR_ ## value)

static void
_inject_const(PythonDestDriver *self, const gchar *field_name, gint value)
{
  PyObject *pyint = py_long_from_long(value);
  PyObject_SetAttrString(self->py.class, field_name, pyint);
  g_ptr_array_add(self->py._refs_to_clean, pyint);
};

static void
_inject_worker_insert_result_consts(PythonDestDriver *self)
{
  _inject_worker_insert_result(self, DROP);
  _inject_worker_insert_result(self, ERROR);
  _inject_worker_insert_result(self, SUCCESS);
  _inject_worker_insert_result(self, QUEUED);
  _inject_worker_insert_result(self, NOT_CONNECTED);
  _inject_worker_insert_result(self, RETRY);
  _inject_worker_insert_result(self, MAX);
};

static PyObject *
py_get_persist_name(PythonDestDriver *self)
{
  return py_string_from_string(python_dd_format_persist_name(&self->super.super.super.super), -1);
}

static gboolean
_py_init_bindings(PythonDestDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super.super);

  self->py._refs_to_clean = g_ptr_array_new_with_free_func((GDestroyNotify)_py_clear);

  self->py.class = _py_resolve_qualified_name(self->binding.class);
  if (!self->py.class)
    {
      gchar buf[256];

      msg_error("python-dest: Error looking up Python driver class",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->binding.class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }

  _inject_worker_insert_result_consts(self);

  PyObject *py_log_template_options = py_log_template_options_new(&self->template_options, cfg);
  PyObject_SetAttrString(self->py.class, "template_options", py_log_template_options);
  Py_DECREF(py_log_template_options);

  PyObject *py_seqnum = py_integer_pointer_new(&self->super.worker.instance.seq_num);
  PyObject_SetAttrString(self->py.class, "seqnum", py_seqnum);
  Py_DECREF(py_seqnum);

  self->py.instance = _py_invoke_function(self->py.class, NULL, self->binding.class, self->super.super.super.id);
  if (!self->py.instance)
    {
      gchar buf1[256], buf2[256];

      msg_error("python-dest: Error instantiating Python driver class",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->binding.class),
                evt_tag_str("class-repr", _py_object_repr(self->py.class, buf1, sizeof(buf1))),
                evt_tag_str("exception", _py_format_exception_text(buf2, sizeof(buf2))));
      _py_finish_exception_handling();
      return FALSE;
    }

  if (!_py_is_log_destination(self->py.instance))
    {
      gchar buf[256];

      if (!cfg_is_config_version_older(cfg, VERSION_VALUE_4_0))
        {
          msg_error("python-dest: Error initializing Python destination, class is not a subclass of LogDestination",
                    evt_tag_str("driver", self->super.super.super.id),
                    evt_tag_str("class", self->binding.class),
                    evt_tag_str("class-repr", _py_object_repr(self->py.class, buf, sizeof(buf))));
          return FALSE;
        }
      msg_warning("WARNING: " VERSION_4_0 " requires that your python() destination class derives "
                  "from syslogng.LogDestination. Please change the class declaration to explicitly "
                  "inherit from syslogng.LogDestination. syslog-ng now operates in compatibility mode",
                  evt_tag_str("driver", self->super.super.super.id),
                  evt_tag_str("class", self->binding.class),
                  evt_tag_str("class-repr", _py_object_repr(self->py.class, buf, sizeof(buf))));

    }
  else
    {
      ((PyLogDestination *) self->py.instance)->driver = self;
    }

  /* these are fast paths, store references to be faster */
  self->py.is_opened = _py_get_attr_or_null(self->py.instance, "is_opened");
  self->py.open = _py_get_attr_or_null(self->py.instance, "open");
  self->py.flush = _py_get_attr_or_null(self->py.instance, "flush");
  self->py.send = _py_get_attr_or_null(self->py.instance, "send");
  self->py.generate_persist_name = _py_get_attr_or_null(self->py.instance, "generate_persist_name");
  if (!self->py.send)
    {
      msg_error("python-dest: Error initializing Python destination, class does not have a send() method",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->binding.class));
      return FALSE;
    }

  PyObject *py_persist_name = py_get_persist_name(self);
  PyObject_SetAttrString(self->py.class, "persist_name", py_persist_name);
  Py_DECREF(py_persist_name);

  g_ptr_array_add(self->py._refs_to_clean, self->py.class);
  g_ptr_array_add(self->py._refs_to_clean, self->py.instance);
  g_ptr_array_add(self->py._refs_to_clean, self->py.is_opened);
  g_ptr_array_add(self->py._refs_to_clean, self->py.open);
  g_ptr_array_add(self->py._refs_to_clean, self->py.flush);
  g_ptr_array_add(self->py._refs_to_clean, self->py.send);
  g_ptr_array_add(self->py._refs_to_clean, self->py.generate_persist_name);

  return TRUE;
}

static void
_py_free_bindings(PythonDestDriver *self)
{
  if (self->py._refs_to_clean)
    g_ptr_array_free(self->py._refs_to_clean, TRUE);
}

static gboolean
_py_init_object(PythonDestDriver *self)
{
  if (!_py_get_attr_or_null(self->py.instance, "init"))
    {
      msg_debug("python-dest: Missing Python method, init()",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->binding.class));
      return TRUE;
    }

  if (!_py_invoke_init(self))
    {
      msg_error("python-dest: Error initializing Python driver object, init() returned FALSE",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->binding.class));
      return FALSE;
    }
  return TRUE;
}

static gboolean
_py_construct_message(PythonDestDriver *self, LogMessage *msg, PyObject **msg_object)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super.super);
  gboolean success;
  *msg_object = NULL;

  if (self->vp)
    {
      LogTemplateEvalOptions options = {&self->template_options, LTZ_LOCAL, self->super.worker.instance.seq_num, NULL, LM_VT_STRING};
      success = py_value_pairs_apply(self->vp, &options, msg, msg_object);
      if (!success && (self->template_options.on_error & ON_ERROR_DROP_MESSAGE))
        return FALSE;
    }
  else
    {
      *msg_object = py_log_message_new(msg, cfg);
    }

  return TRUE;
}

static PyObject *
py_log_destination_stats_written_bytes_add(PyObject *s, PyObject *args)
{
  if (!_py_is_log_destination(s))
    {
      msg_warning_once("stats_written_bytes_add() is not available in compatibility mode");
      Py_RETURN_NONE;
    }

  PyLogDestination *self = (PyLogDestination *) s;

  Py_ssize_t b;

  if (!PyArg_ParseTuple(args, "n", &b))
    return NULL;

  log_threaded_dest_worker_written_bytes_add(&self->driver->super.worker.instance, (gsize) b);
  Py_RETURN_NONE;
}


static LogThreadedResult
python_dd_insert(LogThreadedDestDriver *d, LogMessage *msg)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  LogThreadedResult result = LTR_ERROR;
  PyObject *msg_object;
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  if (self->py.is_opened && !_py_invoke_is_opened(self))
    {
      if (!_py_invoke_open(self))
        {
          result = LTR_NOT_CONNECTED;
          goto exit;
        }
    }

  if (!_py_construct_message(self, msg, &msg_object))
    goto exit;

  result =_py_invoke_send(self, msg_object);
  Py_DECREF(msg_object);

exit:
  PyGILState_Release(gstate);
  return result;
}

static gboolean
python_dd_open(PythonDestDriver *self)
{
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  gboolean retval = _py_invoke_open(self);

  PyGILState_Release(gstate);
  return retval;
}

static LogThreadedResult
python_dd_flush(LogThreadedDestDriver *s)
{
  PythonDestDriver *self = (PythonDestDriver *)s;
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  LogThreadedResult result = _py_invoke_flush(self);
  PyGILState_Release(gstate);
  return result;
};

static void
python_dd_close(PythonDestDriver *self)
{
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  if (self->py.is_opened)
    {
      if (_py_invoke_is_opened(self))
        _py_invoke_close(self);
    }
  else
    {
      _py_invoke_close(self);
    }
  PyGILState_Release(gstate);
}

static gboolean
python_dd_connect(LogThreadedDestDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *) d;

  return python_dd_open(self);
}

static void
python_dd_disconnect(LogThreadedDestDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *) d;

  python_dd_close(self);
}

static gboolean
python_dd_init(LogPipe *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  GlobalConfig *cfg = log_pipe_get_config(d);
  PyGILState_STATE gstate;

  if (!python_binding_init(&self->binding, cfg, self->super.super.super.id))
    return FALSE;

  log_template_options_init(&self->template_options, cfg);
  self->super.time_reopen = 1;

  gstate = PyGILState_Ensure();
  if (!_py_init_bindings(self))
    goto fail;
  PyGILState_Release(gstate);

  if (!log_threaded_dest_driver_init_method(d))
    return FALSE;

  gstate = PyGILState_Ensure();
  if (!_py_init_object(self))
    goto fail;
  PyGILState_Release(gstate);

  msg_verbose("python-dest: Python destination initialized",
              evt_tag_str("driver", self->super.super.super.id),
              evt_tag_str("class", self->binding.class));

  return TRUE;

fail:
  PyGILState_Release(gstate);
  return FALSE;
}

static gboolean
python_dd_deinit(LogPipe *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  _py_invoke_deinit(self);
  PyGILState_Release(gstate);

  python_binding_deinit(&self->binding);

  return log_threaded_dest_driver_deinit_method(d);
}

static void
python_dd_free(LogPipe *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  PyGILState_STATE gstate;

  log_template_options_destroy(&self->template_options);

  gstate = PyGILState_Ensure();
  _py_free_bindings(self);
  PyGILState_Release(gstate);

  value_pairs_unref(self->vp);

  python_binding_clear(&self->binding);
  log_threaded_dest_driver_free(d);
}

LogDriver *
python_dd_new(GlobalConfig *cfg)
{
  PythonDestDriver *self = g_new0(PythonDestDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);
  log_template_options_defaults(&self->template_options);

  self->super.super.super.super.init = python_dd_init;
  self->super.super.super.super.deinit = python_dd_deinit;
  self->super.super.super.super.free_fn = python_dd_free;
  self->super.super.super.super.generate_persist_name = python_dd_format_persist_name;

  self->super.metrics.raw_bytes_enabled = TRUE;

  self->super.worker.connect = python_dd_connect;
  self->super.worker.disconnect = python_dd_disconnect;
  self->super.worker.insert = python_dd_insert;
  self->super.worker.flush = python_dd_flush;

  self->super.format_stats_key = python_dd_format_stats_key;
  self->super.stats_source = stats_register_type("python");

  python_binding_init_instance(&self->binding);

  return (LogDriver *)self;
}

static PyMethodDef py_log_destination_methods[] =
{
  { "stats_written_bytes_add", py_log_destination_stats_written_bytes_add, METH_VARARGS, "Update written_bytes statistics" },
  {NULL}
};

static PyTypeObject py_log_destination_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "LogDestination",
  .tp_basicsize = sizeof(PyLogDestination),
  .tp_dealloc = py_slng_generic_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "The LogDestination class is a base class for custom Python destinations.",
  .tp_new = PyType_GenericNew,
  .tp_methods = py_log_destination_methods,
  0,
};

void
py_log_destination_global_init(void)
{
  PyObject *module = PyImport_AddModule("_syslogng");
  PyObject *enum_seq = PyList_New(6);

  PyList_SetItem(enum_seq, 0, Py_BuildValue("(si)", "DROP", LTR_DROP));
  PyList_SetItem(enum_seq, 1, Py_BuildValue("(si)", "ERROR", LTR_ERROR));
  PyList_SetItem(enum_seq, 2, Py_BuildValue("(si)", "EXPLICIT_ACK_MGMT", LTR_EXPLICIT_ACK_MGMT));
  PyList_SetItem(enum_seq, 3, Py_BuildValue("(si)", "SUCCESS", LTR_SUCCESS));
  PyList_SetItem(enum_seq, 4, Py_BuildValue("(si)", "QUEUED", LTR_QUEUED));
  PyList_SetItem(enum_seq, 5, Py_BuildValue("(si)", "NOT_CONNECTED", LTR_NOT_CONNECTED));
  PyModule_AddObject(module, "LogDestinationResult", _py_construct_enum("LogDestinationResult", enum_seq));

  PyType_Ready(&py_log_destination_type);
  PyModule_AddObject(module, "LogDestination", (PyObject *) &py_log_destination_type);
}
