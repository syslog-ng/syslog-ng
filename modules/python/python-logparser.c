/*
 * Copyright (c) 2014-2016 Balabit
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

#include "python-logparser.h"
#include "python-module.h"
#include "python-logmsg.h"
#include "python-helpers.h"
#include "str-utils.h"
#include "string-list.h"

typedef struct
{
  LogParser super;

  gchar *class;
  GList *loaders;

  GHashTable *options;

  struct
  {
    PyObject *class;
    PyObject *instance;
    PyObject *parser_process;
  } py;
} PythonParser;

void
python_parser_set_class(LogParser *d, gchar *class)
{
  PythonParser *self = (PythonParser *)d;

  g_free(self->class);
  self->class = g_strdup(class);
}

void
python_parser_set_option(LogParser *d, gchar *key, gchar *value)
{
  PythonParser *self = (PythonParser *)d;
  gchar *normalized_key = __normalize_key(key);
  g_hash_table_insert(self->options, normalized_key, g_strdup(value));
}

void
python_parser_set_loaders(LogParser *d, GList *loaders)
{
  PythonParser *self = (PythonParser *)d;

  string_list_free(self->loaders);
  self->loaders = loaders;
}

static gboolean
_pp_py_invoke_bool_function(PythonParser *self, PyObject *func, PyObject *arg)
{
  return _py_invoke_bool_function(func, arg, self->class, self->super.name);
}

static void
_pp_py_invoke_void_method_by_name(PythonParser *self, PyObject *instance, const gchar *method_name)
{
  return _py_invoke_void_method_by_name(instance, method_name, self->class, self->super.name);
}

static gboolean
_pp_py_invoke_bool_method_by_name_with_args(PythonParser *self, PyObject *instance, const gchar *method_name)
{
  return _py_invoke_bool_method_by_name_with_args(instance, method_name, self->options, self->class, self->super.name);
}

static gboolean
_py_invoke_parser_process(PythonParser *self, PyObject *msg)
{
  return _pp_py_invoke_bool_function(self, self->py.parser_process, msg);
}

static gboolean
_py_invoke_init(PythonParser *self)
{
  if (_py_get_attr_or_null(self->py.instance, "init") == NULL)
    return TRUE;
  return _pp_py_invoke_bool_method_by_name_with_args(self, self->py.instance, "init");
}

static void
_py_invoke_deinit(PythonParser *self)
{
  if (_py_get_attr_or_null(self->py.instance, "deinit") != NULL)
    _pp_py_invoke_void_method_by_name(self, self->py.instance, "deinit");
}

static gboolean
_py_init_bindings(PythonParser *self)
{
  self->py.class = _py_resolve_qualified_name(self->class);
  if (!self->py.class)
    {
      gchar buf[256];

      msg_error("Error looking Python parser class",
                evt_tag_str("parser", self->super.name),
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }

  self->py.instance = _py_invoke_function(self->py.class, NULL, self->class, self->super.name);
  if (!self->py.instance)
    {
      gchar buf[256];

      msg_error("Error instantiating Python parser class",
                evt_tag_str("parser", self->super.name),
                evt_tag_str("class", self->class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }

  /* these are fast paths, store references to be faster */
  self->py.parser_process = _py_get_attr_or_null(self->py.instance, "parse");
  if (!self->py.parser_process)
    {
      msg_error("Error initializing Python parser, class does not have a parse() method",
                evt_tag_str("parser", self->super.name),
                evt_tag_str("class", self->class));
    }
  return self->py.parser_process != NULL;
}

static void
_py_free_bindings(PythonParser *self)
{
  Py_CLEAR(self->py.class);
  Py_CLEAR(self->py.instance);
  Py_CLEAR(self->py.parser_process);
}

static gboolean
_py_init_object(PythonParser *self)
{
  if (!_py_invoke_init(self))
    {
      msg_error("Error initializing Python parser object, init() returned FALSE",
                evt_tag_str("parser", self->super.name),
                evt_tag_str("class", self->class));
      return FALSE;
    }
  return TRUE;
}

static gboolean
python_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
                      gsize input_len)
{
  PythonParser *self = (PythonParser *)s;
  PyGILState_STATE gstate;
  gboolean result;

  gstate = PyGILState_Ensure();
  {
    LogMessage *msg = log_msg_make_writable(pmsg, path_options);

    msg_debug("Invoking the Python parse() method",
              evt_tag_str("parser", self->super.name),
              evt_tag_str("class", self->class),
              log_pipe_location_tag(&self->super.super),
              evt_tag_printf("msg", "%p", msg));

    PyObject *msg_object = py_log_message_new(msg);
    result = _py_invoke_parser_process(self, msg_object);
    Py_DECREF(msg_object);
  }
  PyGILState_Release(gstate);

  return result;
}

static gboolean
python_parser_init(LogPipe *s)
{
  PythonParser *self = (PythonParser *)s;
  PyGILState_STATE gstate;

  if (!self->class)
    {
      msg_error("Error initializing Python parser: no script specified!",
                evt_tag_str("parser", self->super.name));
      return FALSE;
    }

  if (!log_parser_init_method(s))
    return FALSE;

  gstate = PyGILState_Ensure();

  _py_perform_imports(self->loaders);
  if (!_py_init_bindings(self) ||
      !_py_init_object(self))
    goto fail;

  PyGILState_Release(gstate);

  msg_verbose("Python parser initialized",
              evt_tag_str("parser", self->super.name),
              evt_tag_str("class", self->class));

  return TRUE;
fail:
  PyGILState_Release(gstate);
  return FALSE;
}

static gboolean
python_parser_deinit(LogPipe *d)
{
  PythonParser *self = (PythonParser *)d;
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  _py_invoke_deinit(self);
  PyGILState_Release(gstate);

  return log_parser_deinit_method(d);
}

static void
python_parser_free(LogPipe *d)
{
  PythonParser *self = (PythonParser *)d;
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  _py_free_bindings(self);
  PyGILState_Release(gstate);

  g_free(self->class);

  if (self->options)
    g_hash_table_unref(self->options);

  string_list_free(self->loaders);

  log_parser_free_method(d);
}

static LogPipe *
python_parser_clone(LogPipe *s)
{
  PythonParser *self = (PythonParser *) s;
  PythonParser *cloned = (PythonParser *) python_parser_new(log_pipe_get_config(s));
  g_hash_table_unref(cloned->options);
  python_parser_set_class(&cloned->super, self->class);
  cloned->loaders = string_list_clone(self->loaders);
  cloned->options = g_hash_table_ref(self->options);

  return &cloned->super.super;
}

LogParser *
python_parser_new(GlobalConfig *cfg)
{
  PythonParser *self = g_new0(PythonParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.init = python_parser_init;
  self->super.super.deinit = python_parser_deinit;
  self->super.super.free_fn = python_parser_free;
  self->super.super.clone = python_parser_clone;
  self->super.process = python_parser_process;
  self->py.class = self->py.instance = self->py.parser_process = NULL;

  self->options = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  return (LogParser *)self;
}
