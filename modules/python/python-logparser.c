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

  PythonBinding binding;
  struct
  {
    PyObject *class;
    PyObject *instance;
    PyObject *parser_process;
  } py;
} PythonParser;

typedef struct _PyLogParser
{
  PyObject_HEAD
} PyLogParser;

static PyTypeObject py_log_parser_type;

static gboolean
_py_is_log_parser(PyObject *obj)
{
  return PyType_IsSubtype(Py_TYPE(obj), &py_log_parser_type);
}

PythonBinding *
python_parser_get_binding(LogParser *d)
{
  PythonParser *self = (PythonParser *)d;

  return &self->binding;
}

static gboolean
_pp_py_invoke_bool_function(PythonParser *self, PyObject *func, PyObject *arg)
{
  return _py_invoke_bool_function(func, arg, self->binding.class, self->super.name);
}

static void
_pp_py_invoke_void_method_by_name(PythonParser *self, PyObject *instance, const gchar *method_name)
{
  return _py_invoke_void_method_by_name(instance, method_name, self->binding.class, self->super.name);
}

static gboolean
_pp_py_invoke_bool_method_by_name_with_options(PythonParser *self, PyObject *instance, const gchar *method_name)
{
  return _py_invoke_bool_method_by_name_with_options(instance, method_name, self->binding.options,
                                                     self->binding.class, self->super.name);
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
  return _pp_py_invoke_bool_method_by_name_with_options(self, self->py.instance, "init");
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
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super);


  self->py.class = _py_resolve_qualified_name(self->binding.class);
  if (!self->py.class)
    {
      gchar buf[256];

      msg_error("Error looking Python parser class",
                evt_tag_str("parser", self->super.name),
                evt_tag_str("class", self->binding.class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }

  self->py.instance = _py_invoke_function(self->py.class, NULL, self->binding.class, self->super.name);
  if (!self->py.instance)
    {
      gchar buf[256];

      msg_error("Error instantiating Python parser class",
                evt_tag_str("parser", self->super.name),
                evt_tag_str("class", self->binding.class),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }

  if (!_py_is_log_parser(self->py.instance))
    {
      gchar buf[256];

      if (!cfg_is_config_version_older(cfg, VERSION_VALUE_4_0))
        {
          msg_error("python-parser: Error initializing Python parser, class is not a subclass of LogParser",
                    evt_tag_str("parser", self->super.name),
                    evt_tag_str("class", self->binding.class),
                    evt_tag_str("class-repr", _py_object_repr(self->py.class, buf, sizeof(buf))));
          return FALSE;
        }
      msg_warning("WARNING: " VERSION_4_0 " requires that your python() parser class derives "
                  "from syslogng.LogParser. Please change the class declaration to explicitly "
                  "inherit from syslogng.LogParser. syslog-ng now operates in compatibility mode",
                  evt_tag_str("parser", self->super.name),
                  evt_tag_str("class", self->binding.class),
                  evt_tag_str("class-repr", _py_object_repr(self->py.class, buf, sizeof(buf))));

    }


  /* these are fast paths, store references to be faster */
  self->py.parser_process = _py_get_attr_or_null(self->py.instance, "parse");
  if (!self->py.parser_process)
    {
      msg_error("Error initializing Python parser, class does not have a parse() method",
                evt_tag_str("parser", self->super.name),
                evt_tag_str("class", self->binding.class));
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
                evt_tag_str("class", self->binding.class));
      return FALSE;
    }
  return TRUE;
}

static gboolean
python_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
                      gsize input_len)
{
  PythonParser *self = (PythonParser *)s;
  GlobalConfig *cfg = log_pipe_get_config(&s->super);
  PyGILState_STATE gstate;
  gboolean result;

  gstate = PyGILState_Ensure();
  {
    LogMessage *msg = log_msg_make_writable(pmsg, path_options);

    msg_trace("python-parser message processing started",
              evt_tag_str("input", input),
              evt_tag_str("parser", self->super.name),
              evt_tag_str("class", self->binding.class),
              evt_tag_msg_reference(msg));

    PyObject *msg_object = py_log_message_new(msg, cfg);
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
  GlobalConfig *cfg = log_pipe_get_config(s);
  PyGILState_STATE gstate;

  if (!log_parser_init_method(s))
    return FALSE;

  if (!python_binding_init(&self->binding, cfg, self->super.name))
    return FALSE;

  gstate = PyGILState_Ensure();

  if (!_py_init_bindings(self) ||
      !_py_init_object(self))
    goto fail;

  PyGILState_Release(gstate);

  msg_verbose("Python parser initialized",
              evt_tag_str("parser", self->super.name),
              evt_tag_str("class", self->binding.class));

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

  python_binding_deinit(&self->binding);
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

  python_binding_clear(&self->binding);
  log_parser_free_method(d);
}

static LogPipe *
python_parser_clone(LogPipe *s)
{
  PythonParser *self = (PythonParser *) s;
  PythonParser *cloned = (PythonParser *) python_parser_new(log_pipe_get_config(s));
  log_parser_clone_settings(&self->super, &cloned->super);
  python_binding_clone(&self->binding, &cloned->binding);
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

  python_binding_init_instance(&self->binding);

  return (LogParser *)self;
}

static PyTypeObject py_log_parser_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "LogDestination",
  .tp_basicsize = sizeof(PyLogParser),
  .tp_dealloc = py_slng_generic_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "The LogDestination class is a base class for custom Python sources.",
  .tp_new = PyType_GenericNew,
  0,
};

void
py_log_parser_global_init(void)
{
  PyType_Ready(&py_log_parser_type);
  PyModule_AddObject(PyImport_AddModule("_syslogng"), "LogParser", (PyObject *) &py_log_parser_type);
}
