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
#include "python-helpers.h"
#include "logthrdestdrv.h"
#include "stats/stats.h"
#include "string-list.h"
#include "str-utils.h"
#include "messages.h"

typedef struct
{
  LogThrDestDriver super;

  gchar *class;
  GList *loaders;

  LogTemplateOptions template_options;
  GHashTable *options;
  ValuePairs *vp;

  struct
  {
    PyObject *class;
    PyObject *instance;
    PyObject *is_opened;
    PyObject *retry_error;
    PyObject *send;
  } py;
} PythonDestDriver;

/** Setters & config glue **/

void
python_dd_set_class(LogDriver *d, gchar *filename)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  g_free(self->class);
  self->class = g_strdup(filename);
}

void
python_dd_set_option(LogDriver *d, gchar *key, gchar *value)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  gchar *normalized_key = __normalize_key(key);
  g_hash_table_insert(self->options, normalized_key, g_strdup(value));
}

void
python_dd_set_value_pairs(LogDriver *d, ValuePairs *vp)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  value_pairs_unref(self->vp);
  self->vp = vp;
}

void
python_dd_set_loaders(LogDriver *d, GList *loaders)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  string_list_free(self->loaders);
  self->loaders = loaders;
}

PyObject *
python_dd_create_arg_dict(PythonDestDriver *self)
{
  return _py_create_arg_dict(self->options);
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

  if (d->super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "python,%s", d->super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "python,%s", self->class);

  return persist_name;
}

static const gchar *
python_dd_format_persist_name(const LogPipe *s)
{
  const PythonDestDriver *self = (const PythonDestDriver *)s;
  static gchar persist_name[1024];

  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "python.%s", s->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "python(%s)", self->class);

  return persist_name;
}

static gboolean
_dd_py_invoke_bool_function(PythonDestDriver *self, PyObject *func, PyObject *arg)
{
  return _py_invoke_bool_function(func, arg, self->class, self->super.super.super.id);
}

static void
_dd_py_invoke_void_function(PythonDestDriver *self, PyObject *func, PyObject *arg)
{
  _py_invoke_void_function(func, arg, self->class, self->super.super.super.id);
}


static void
_dd_py_invoke_void_method_by_name(PythonDestDriver *self, const gchar *method_name)
{
  _py_invoke_void_method_by_name(self->py.instance, method_name, self->class, self->super.super.super.id);
}

static gboolean
_dd_py_invoke_bool_method_by_name_with_args(PythonDestDriver *self, const gchar *method_name)
{
  return _py_invoke_bool_method_by_name_with_args(self->py.instance, method_name, self->options, self->class,
                                                  self->super.super.super.id);
}

static gboolean
_dd_py_invoke_bool_method_by_name(PythonDestDriver *self, const gchar *method_name)
{
  return _py_invoke_bool_method_by_name_with_args(self->py.instance, method_name, NULL, self->class,
                                                  self->super.super.super.id);
}

static gboolean
_py_invoke_is_opened(PythonDestDriver *self)
{
  if (!self->py.is_opened)
    return TRUE;

  return _dd_py_invoke_bool_function(self, self->py.is_opened, NULL);
}

static void
_py_invoke_retry_error(PythonDestDriver *self, PyObject *dict)
{
  if (!self->py.retry_error)
    return;
  _dd_py_invoke_void_function(self, self->py.retry_error, dict);
}

static gboolean
_py_invoke_open(PythonDestDriver *self)
{
  return _dd_py_invoke_bool_method_by_name(self, "open");
}

static void
_py_invoke_close(PythonDestDriver *self)
{
  _dd_py_invoke_void_method_by_name(self, "close");
}

static gboolean
_py_invoke_send(PythonDestDriver *self, PyObject *dict)
{
  return _dd_py_invoke_bool_function(self, self->py.send, dict);
}

static gboolean
_py_invoke_init(PythonDestDriver *self)
{
  return _dd_py_invoke_bool_method_by_name_with_args(self, "init");
}

static void
_py_invoke_deinit(PythonDestDriver *self)
{
  _dd_py_invoke_void_method_by_name(self, "deinit");
}

static gboolean
_py_init_bindings(PythonDestDriver *self)
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

  /* these are fast paths, store references to be faster */
  self->py.is_opened = _py_get_attr_or_null(self->py.instance, "is_opened");
  self->py.retry_error = _py_get_attr_or_null(self->py.instance, "retry_error");
  self->py.send = _py_get_attr_or_null(self->py.instance, "send");
  if (!self->py.send)
    {
      msg_error("Error initializing Python destination, class does not have a send() method",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->class));
    }
  return self->py.send != NULL;
}

static void
_py_free_bindings(PythonDestDriver *self)
{
  Py_CLEAR(self->py.class);
  Py_CLEAR(self->py.instance);
  Py_CLEAR(self->py.is_opened);
  Py_CLEAR(self->py.send);
  Py_CLEAR(self->py.retry_error);
}

static gboolean
_py_init_object(PythonDestDriver *self)
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
_py_construct_message(PythonDestDriver *self, LogMessage *msg, PyObject **msg_object)
{
  gboolean success;
  *msg_object = NULL;

  if (self->vp)
    {
      success = py_value_pairs_apply(self->vp, &self->template_options, self->super.seq_num, msg, msg_object);
      if (!success && (self->template_options.on_error & ON_ERROR_DROP_MESSAGE))
        return FALSE;
    }
  else
    {
      *msg_object = py_log_message_new(msg);
    }

  return TRUE;
}


static worker_insert_result_t
python_dd_insert(LogThrDestDriver *d, LogMessage *msg)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  worker_insert_result_t result = WORKER_INSERT_RESULT_ERROR;
  PyObject *msg_object;
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  if (!_py_invoke_is_opened(self))
    {
      _py_invoke_open(self);
      if (!_py_invoke_is_opened(self))
        {
          result = WORKER_INSERT_RESULT_NOT_CONNECTED;
          goto exit;
        }
    }

  if (!_py_construct_message(self, msg, &msg_object))
    goto exit;

  if (_py_invoke_send(self, msg_object))
    {
      result = WORKER_INSERT_RESULT_SUCCESS;
    }
  else
    {
      msg_error("Python send() method returned failure, suspending destination for time_reopen()",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("class", self->class),
                evt_tag_int("time_reopen", self->super.time_reopen));
    }
  Py_DECREF(msg_object);

exit:
  PyGILState_Release(gstate);
  return result;
}

static void
python_dd_open(PythonDestDriver *self)
{
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  if (!_py_invoke_is_opened(self))
    _py_invoke_open(self);

  PyGILState_Release(gstate);
}

static void
python_dd_retry_error(PythonDestDriver *self, LogMessage *msg)
{
  PyGILState_STATE gstate;
  PyObject *msg_object;

  gstate = PyGILState_Ensure();
  if(_py_construct_message(self, msg, &msg_object))
    {
      _py_invoke_retry_error(self, msg_object);
      Py_DECREF(msg_object);
    }

  PyGILState_Release(gstate);
}


static void
python_dd_close(PythonDestDriver *self)
{
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  if (_py_invoke_is_opened(self))
    _py_invoke_close(self);
  PyGILState_Release(gstate);
}

static void
python_dd_worker_init(LogThrDestDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;

  python_dd_open(self);
}

static void
python_dd_disconnect(LogThrDestDriver *d)
{
  PythonDestDriver *self = (PythonDestDriver *) d;

  python_dd_close(self);
}

static void
python_dd_over_message(LogThrDestDriver *s, LogMessage *msg)
{
  PythonDestDriver *self = (PythonDestDriver *)s;
  python_dd_retry_error(self, msg);
}

static gboolean
python_dd_init(LogPipe *d)
{
  PythonDestDriver *self = (PythonDestDriver *)d;
  GlobalConfig *cfg = log_pipe_get_config(d);
  PyGILState_STATE gstate;

  if (!self->class)
    {
      msg_error("Error initializing Python destination: no script specified!",
                evt_tag_str("driver", self->super.super.super.id));
      return FALSE;
    }

  if (!log_dest_driver_init_method(d))
    return FALSE;

  log_template_options_init(&self->template_options, cfg);
  self->super.time_reopen = 1;

  gstate = PyGILState_Ensure();

  _py_perform_imports(self->loaders);
  if (!_py_init_bindings(self) ||
      !_py_init_object(self))
    goto fail;

  PyGILState_Release(gstate);

  msg_verbose("Python destination initialized",
              evt_tag_str("driver", self->super.super.super.id),
              evt_tag_str("class", self->class));

  return log_threaded_dest_driver_init_method(d);

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

  g_free(self->class);

  value_pairs_unref(self->vp);

  if (self->options)
    g_hash_table_unref(self->options);

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

  self->super.messages.retry_over = python_dd_over_message;

  self->super.worker.thread_init = python_dd_worker_init;
  self->super.worker.disconnect = python_dd_disconnect;
  self->super.worker.insert = python_dd_insert;

  self->super.format.stats_instance = python_dd_format_stats_instance;
  self->super.stats_source = SCS_PYTHON;

  self->options = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  return (LogDriver *)self;
}
