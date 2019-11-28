/*
 * Copyright (c) 2019 Balabit
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

#include "python-persist.h"
#include "python-helpers.h"
#include "syslog-ng.h"
#include "driver.h"
#include "mainloop.h"

#include <structmember.h>

static PyObject *
_call_generate_persist_name_method(PythonPersistMembers *options)
{
  PyObject *py_options = options->options ? _py_create_arg_dict(options->options) : NULL;
  PyObject *ret = _py_invoke_function(options->generate_persist_name_method, py_options,
                                      options->class, options->id);
  Py_XDECREF(py_options);
  return ret;
}

static void
format_default_stats_instance(gchar *buffer, gsize size, const gchar *module, const gchar *name)
{
  g_snprintf(buffer, size, "%s,%s", module, name);
}

static void
copy_stats_instance(const LogPipe *self, const gchar *module, PythonPersistMembers *options,
                    gchar *buffer, gsize size)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyObject *ret = _call_generate_persist_name_method(options);
  if (ret)
    g_snprintf(buffer, size, "%s,%s", module, _py_get_string_as_string(ret));
  else
    {
      format_default_stats_instance(buffer, size, module, options->class);
      msg_error("Failed while generating persist name, using default",
                evt_tag_str("default_persist_name", buffer),
                evt_tag_str("driver", options->id),
                evt_tag_str("class", options->class));
    }
  Py_XDECREF(ret);

  PyGILState_Release(gstate);
}

const gchar *
python_format_stats_instance(LogPipe *p, const gchar *module, PythonPersistMembers *options)
{
  static gchar persist_name[1024];

  if (p->persist_name)
    format_default_stats_instance(persist_name, sizeof(persist_name), module, p->persist_name);
  else if (options->generate_persist_name_method)
    copy_stats_instance(p, module, options, persist_name, sizeof(persist_name));
  else
    format_default_stats_instance(persist_name, sizeof(persist_name), module, options->class);

  return persist_name;
}

static void
format_default_persist_name_with_persist_name(gchar *buffer, gsize size, const gchar *module, const gchar *persist_name)
{
  g_snprintf(buffer, size, "%s.%s", module, persist_name);
}

static void
format_default_persist_name_with_class(gchar *buffer, gsize size, const gchar *module, const gchar *class)
{
  g_snprintf(buffer, size, "%s(%s)", module, class);
}

static void
copy_persist_name(const LogPipe *self, const gchar *module, PythonPersistMembers *options,
                  gchar *buffer, gsize size)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyObject *ret =_call_generate_persist_name_method(options);
  if (ret)
    g_snprintf(buffer, size, "%s.%s", module, _py_get_string_as_string(ret));
  else
    {
      format_default_persist_name_with_class(buffer, size, module, options->class);
      msg_error("Failed while generating persist name, using default",
                evt_tag_str("default_persist_name", buffer),
                evt_tag_str("driver", options->id),
                evt_tag_str("class", options->class));
    }
  Py_XDECREF(ret);

  PyGILState_Release(gstate);
}

const gchar *
python_format_persist_name(const LogPipe *p, const gchar *module, PythonPersistMembers *options)
{
  static gchar persist_name[1024];

  if (p->persist_name)
    format_default_persist_name_with_persist_name(persist_name, sizeof(persist_name), module, p->persist_name);
  else if (options->generate_persist_name_method)
    copy_persist_name(p, module, options, persist_name, sizeof(persist_name));
  else
    format_default_persist_name_with_class(persist_name, sizeof(persist_name), module, options->class);

  return persist_name;
}

static int
_persist_type_init(PyObject *s, PyObject *args, PyObject *kwds)
{
  PyPersist *self =(PyPersist *)s;
  const gchar *persist_name=NULL;

  GlobalConfig *cfg = main_loop_get_current_config(main_loop_get_instance());
  self->persist_state = cfg->state;

  static char *kwlist[] = {"persist_name", NULL};

  if (! PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &persist_name))
    return -1;

  if (g_strstr_len(persist_name, -1, "##"))
    {
      // Internally, we will store subkeys as persist_name##subkey
      PyErr_Format(PyExc_ValueError, "persist name cannot contain ##");
      return -1;
    }

  if (!self->persist_name)
    self->persist_name = g_strdup(persist_name);

  return 0;
}

static void
py_persist_dealloc(PyObject *s)
{
  PyPersist *self =(PyPersist *)s;

  g_free(self->persist_name);
  self->persist_name = NULL;

  py_slng_generic_dealloc(s);
}

static PyMemberDef
py_persist_type_members[] =
{
  {"persist_name", T_STRING, offsetof(PyPersist, persist_name), READONLY},
  {NULL}
};

static gchar *
_build_key(PyPersist *self, const gchar *key)
{
  return g_strdup_printf("%s##%s", self->persist_name, key);
}

static gchar *
_lookup_entry(PyPersist *self, const gchar *key)
{
  gchar *query_key = _build_key(self, key);

  gsize size;
  guint8 version;
  PersistEntryHandle handle = persist_state_lookup_entry(self->persist_state, query_key, &size, &version);
  if (!handle)
    {
      PyErr_Format(PyExc_KeyError, "Persist has no such key: %s", key);
      g_free(query_key);
      return NULL;
    }

  const gchar *data = persist_state_map_entry(self->persist_state, handle);
  gchar *copy = g_strdup((gchar *)data);
  persist_state_unmap_entry(self->persist_state, handle);

  g_free(query_key);

  return copy;
};

static PersistEntryHandle
_allocate_persist_entry(PersistState *persist_state, const gchar *key, const gchar *value, gsize value_len)
{
  gsize size;
  guint8 version;
  PersistEntryHandle handle = persist_state_lookup_entry(persist_state, key, &size, &version);
  if (handle && size >= value_len)
    return handle;

  return persist_state_alloc_entry(persist_state, key, value_len);
}

static gboolean
_store_entry(PyPersist *self, const gchar *key, const gchar *value)
{
  gchar *query_key = _build_key(self, key);
  gsize value_len = strlen(value);

  PersistEntryHandle handle = _allocate_persist_entry(self->persist_state, query_key, value, value_len);
  if (!handle)
    {
      g_free(query_key);
      return FALSE;
    }

  gchar *data = persist_state_map_entry(self->persist_state, handle);
  memcpy(data, value, value_len);
  persist_state_unmap_entry(self->persist_state, handle);

  g_free(query_key);

  return TRUE;
}

static PyObject *
_py_persist_type_get(PyObject *o, PyObject *key)
{
  PyPersist *self = (PyPersist *)o;

  if (!_py_is_string(key))
    {
      PyErr_SetString(PyExc_TypeError, "key is not a string object");
      return NULL;
    }

  const gchar *name = _py_get_string_as_string(key);
  gchar *value = _lookup_entry(self, name);

  if (!value)
    {
      PyErr_Format(PyExc_KeyError, "No such name-value pair %s", name);
      return NULL;
    }

  PyObject *py_value =  _py_string_from_string(value, -1);
  g_free(value);
  return py_value;
}

static int
_py_persist_type_set(PyObject *o, PyObject *k, PyObject *v)
{
  PyPersist *self = (PyPersist *)o;

  if (!_py_is_string(k))
    {
      PyErr_SetString(PyExc_TypeError, "key is not a string object");
      return -1;
    }

  if (!_py_is_string(v))
    {
      PyErr_SetString(PyExc_TypeError, "value is not a string object");
      return -1;
    }

  const gchar *key = _py_get_string_as_string(k);
  const gchar *value = _py_get_string_as_string(v);

  if (!_store_entry(self, key, value))
    {
      PyErr_SetString(PyExc_IOError, "value could not be stored");
      return -1;
    }

  return 0;
}

static PyMappingMethods py_persist_type_mapping =
{
  .mp_length = NULL,
  .mp_subscript = (binaryfunc) _py_persist_type_get,
  .mp_ass_subscript = (objobjargproc) _py_persist_type_set
};


PyTypeObject py_persist_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "Persist",
  .tp_basicsize = sizeof(PyPersist),
  .tp_dealloc = py_persist_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "Persist class encapsulates persist handling",
  .tp_new = PyType_GenericNew,
  .tp_init = _persist_type_init,
  .tp_members = py_persist_type_members,
  .tp_as_mapping = &py_persist_type_mapping,
  0,
};

void
py_persist_init(void)
{
  PyType_Ready(&py_persist_type);
  PyModule_AddObject(PyImport_AddModule("_syslogng"), "Persist", (PyObject *) &py_persist_type);
}
