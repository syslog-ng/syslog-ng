/*
 * Copyright (c) 2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2014 Gergely Nagy <algernon@balabit.hu>
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
#include "python-globals.h"
#include "python-value-pairs.h"
#include "logthrdestdrv.h"
#include "stats/stats.h"
#include "misc.h"

#ifndef SCS_PYTHON
#define SCS_PYTHON 0
#endif

typedef struct
{
  LogThrDestDriver super;

  gchar *filename;
  gchar *init_func_name;
  gchar *queue_func_name;
  gchar *deinit_func_name;
  GList *imports;

  LogTemplateOptions template_options;
  ValuePairs *vp;

  struct
  {
    PyObject *module;
    PyObject *init;
    PyObject *queue;
    PyObject *deinit;
  } py;
} PythonDestDriver;

/** Setters & config glue **/

void
python_dd_set_init_func(LogDriver *d, gchar *init_func_name)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  g_free(self->init_func_name);
  self->init_func_name = g_strdup(init_func_name);
}

void
python_dd_set_queue_func(LogDriver *d, gchar *queue_func_name)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  g_free(self->queue_func_name);
  self->queue_func_name = g_strdup(queue_func_name);
}

void
python_dd_set_deinit_func(LogDriver *d, gchar *deinit_func_name)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  g_free(self->deinit_func_name);
  self->deinit_func_name = g_strdup(deinit_func_name);
}

void
python_dd_set_filename(LogDriver *d, gchar *filename)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  g_free(self->filename);
  self->filename = g_strdup(filename);
}

void
python_dd_set_value_pairs(LogDriver *d, ValuePairs *vp)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  if (self->vp)
    value_pairs_free(self->vp);
  self->vp = vp;
}

void
python_dd_set_imports(LogDriver *d, GList *imports)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  string_list_free(self->imports);
  self->imports = imports;
}

LogTemplateOptions *
python_dd_get_template_options(LogDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  return &self->template_options;
}

/** Helpers for stats & persist_name formatting **/

static gchar *
python_dd_format_stats_instance(LogThrDestDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
             "python,%s,%s,%s,%s",
             self->filename,
             self->init_func_name,
             self->queue_func_name,
             self->deinit_func_name);
  return persist_name;
}

static gchar *
python_dd_format_persist_name(LogThrDestDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
             "python(%s,%s,%s,%s)",
             self->filename,
             self->init_func_name,
             self->queue_func_name,
             self->deinit_func_name);
  return persist_name;
}

/** Python calling helpers **/
static gboolean
_py_function_return_value_as_bool(PythonDestDriver *self,
                                  const gchar *func_name,
                                  PyObject *ret)
{
  if (!ret)
    {
      msg_error("Python function returned NULL",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("script", self->filename),
                evt_tag_str("function", func_name),
                NULL);
      return FALSE;
    }

  if (ret == Py_None)
    return TRUE;

  if (!PyBool_Check(ret))
    {
      msg_error("Python function returned a non-bool value",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("script", self->filename),
                evt_tag_str("function", func_name),
                NULL);
      Py_DECREF(ret);
      return FALSE;
    }

  if (PyLong_AsLong(ret) != 1)
    {
      msg_error("Python function returned FALSE",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("script", self->filename),
                evt_tag_str("function", func_name),
                NULL);
      Py_DECREF(ret);
      return FALSE;
    }

  Py_DECREF(ret);
  return TRUE;
}

static gboolean
_call_python_function_with_no_args_and_bool_return_value(PythonDestDriver *self,
                                                         const gchar *func_name,
                                                         PyObject *func)
{
  PyObject *ret;
  gboolean success;

  if (!func)
    return TRUE;

  ret = PyObject_CallObject(func, NULL);
  success = _py_function_return_value_as_bool(self, func_name, ret);
  return success;
}

static gboolean
_py_call_function_with_arguments(PythonDestDriver *self,
                                 const gchar *func_name, PyObject *func,
                                 PyObject *func_args)
{
  gboolean success;
  PyObject *ret;

  ret = PyObject_CallObject(func, func_args);
  success = _py_function_return_value_as_bool(self, func_name, ret);

  Py_DECREF(func_args);
  if (ret != Py_None)
    Py_DECREF(ret);

  return success;
}

static worker_insert_result_t
python_worker_eval(LogThrDestDriver *d, LogMessage *msg)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  gboolean success, vp_ok;
  PyObject *func_args, *dict;
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();

  func_args = PyTuple_New(1);
  vp_ok = py_value_pairs_apply(self->vp, &self->template_options, self->super.seq_num, msg, &dict);
  PyTuple_SetItem(func_args, 0, dict);

  if (!vp_ok && (self->template_options.on_error & ON_ERROR_DROP_MESSAGE))
    {
      Py_DECREF(func_args);
      goto exit;
    }

  success = _py_call_function_with_arguments(self,
                                             self->queue_func_name, self->py.queue,
                                             func_args);
  if (!success)
    {
      msg_error("Error while calling a Python function",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("script", self->filename),
                evt_tag_str("function", self->queue_func_name),
                NULL);
    }

 exit:

  PyGILState_Release(gstate);

  if (success && vp_ok)
    {
      return WORKER_INSERT_RESULT_SUCCESS;
    }
  else
    {
      return WORKER_INSERT_RESULT_DROP;
    }
}

static void
_py_do_import(gpointer data, gpointer user_data)
{
  gchar *modname = (gchar *)data;
  PythonDestDriver *self = (PythonDestDriver *)user_data;
  PyObject *module, *modobj;

  module = PyUnicode_FromString(modname);
  if (!module)
    {
      msg_error("Error allocating Python string",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("string", modname),
                NULL);
      return;
    }

  modobj = PyImport_Import(module);
  Py_DECREF(module);
  if (!modobj)
    {
      msg_error("Error loading Python module",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("module", modname),
                NULL);
      return;
    }
  Py_DECREF(modobj);
}

static gboolean
python_worker_init(LogPipe *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  GlobalConfig *cfg = log_pipe_get_config(d);
  PyObject *modname;
  PyGILState_STATE gstate;

  if (!self->filename)
    {
      msg_error("Error initializing Python destination: no script specified!",
                evt_tag_str("driver", self->super.super.super.id),
                NULL);
      return FALSE;
    }

  if (!log_dest_driver_init_method(d))
    return FALSE;

  log_template_options_init(&self->template_options, cfg);

  if (!self->queue_func_name)
    self->queue_func_name = g_strdup("queue");

  gstate = PyGILState_Ensure();

  g_list_foreach(self->imports, _py_do_import, self);

  modname = PyUnicode_FromString(self->filename);
  if (!modname)
    {
      msg_error("Unable to convert filename to Python string",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("script", self->filename),
                NULL);
      PyGILState_Release(gstate);
      return FALSE;
    }

  self->py.module = PyImport_Import(modname);
  Py_DECREF(modname);

  if (!self->py.module)
    {
      msg_error("Unable to load Python script",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("script", self->filename),
                NULL);
      PyGILState_Release(gstate);
      return FALSE;
    }

  self->py.queue = PyObject_GetAttrString(self->py.module,
                                          self->queue_func_name);
  if (!self->py.queue || !PyCallable_Check(self->py.queue))
    {
      msg_error("Python queue function is not callable!",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("script", self->filename),
                evt_tag_str("queue-function", self->queue_func_name),
                NULL);
      Py_DECREF(self->py.module);
      PyGILState_Release(gstate);
      return FALSE;
    }

  if (self->init_func_name)
    self->py.init = PyObject_GetAttrString(self->py.module,
                                           self->init_func_name);
  if (self->py.init && !PyCallable_Check(self->py.init))
    {
      Py_DECREF(self->py.init);
      self->py.init = NULL;
    }

  if (self->deinit_func_name)
    self->py.deinit = PyObject_GetAttrString(self->py.module,
                                             self->deinit_func_name);
  if (self->py.deinit && !PyCallable_Check(self->py.deinit))
    {
      Py_DECREF(self->py.deinit);
      self->py.deinit = NULL;
    }

  if (self->py.init)
    {
      if (!_call_python_function_with_no_args_and_bool_return_value(self,
                                                                    self->init_func_name,
                                                                    self->py.init))
        {
          if (self->py.init)
            Py_DECREF(self->py.init);
          if (self->py.deinit)
            Py_DECREF(self->py.deinit);
          Py_DECREF(self->py.queue);
          Py_DECREF(self->py.module);
          PyGILState_Release(gstate);
          return FALSE;
        }
    }

  PyGILState_Release(gstate);

  msg_verbose("Initializing Python destination",
              evt_tag_str("driver", self->super.super.super.id),
              evt_tag_str("script", self->filename),
              NULL);

  return log_threaded_dest_driver_start(d);
}

static gboolean
python_worker_deinit(LogPipe *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();

  if (self->py.deinit)
    {
      if (!_call_python_function_with_no_args_and_bool_return_value(self,
                                                                    self->deinit_func_name,
                                                                    self->py.deinit))
        {
          PyGILState_Release(gstate);
          return FALSE;
        }
    }

  PyGILState_Release(gstate);

  return log_threaded_dest_driver_deinit_method(d);
}

static void
python_dd_free(LogPipe *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  log_template_options_destroy(&self->template_options);

  g_free(self->filename);
  g_free(self->init_func_name);
  g_free(self->queue_func_name);
  g_free(self->deinit_func_name);

  if (self->vp)
    value_pairs_free(self->vp);

  log_threaded_dest_driver_free(d);
}

LogDriver *
python_dd_new(GlobalConfig *cfg)
{
  PythonDestDriver *self = g_new0(PythonDestDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  self->super.super.super.super.init = python_worker_init;
  self->super.super.super.super.deinit = python_worker_deinit;
  self->super.super.super.super.free_fn = python_dd_free;

  self->super.worker.disconnect = NULL;
  self->super.worker.insert = python_worker_eval;

  self->super.format.stats_instance = python_dd_format_stats_instance;
  self->super.format.persist_name = python_dd_format_persist_name;
  self->super.stats_source = SCS_PYTHON;

  log_template_options_defaults(&self->template_options);
  python_dd_set_value_pairs(&self->super.super.super, value_pairs_new_default(cfg));

  return (LogDriver *)self;
}
