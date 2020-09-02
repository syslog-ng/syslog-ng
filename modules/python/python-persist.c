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
#include "persistable-state-header.h"
#include "python-helpers.h"
#include "syslog-ng.h"
#include "driver.h"
#include "mainloop.h"
#include "compat/compat-python.h"

#include <structmember.h>

#define SUBKEY_DELIMITER "##"
#define MASTER_ENTRY_VERSION 1

typedef PersistableStateHeader PythonPersistMasterEntry;

typedef enum
{
  ENTRY_TYPE_STRING,
  ENTRY_TYPE_LONG,
  ENTRY_TYPE_BYTES,
  ENTRY_TYPE_MAX
} EntryType;

typedef struct
{
  guint8 type;
  gchar data[0];
} Entry;

/* Ensure there is no padding between type and data */
G_STATIC_ASSERT(offsetof(Entry, data) == sizeof(guint8) + offsetof(Entry, type));

static PyObject *
entry_to_pyobject(guint8 type, gchar *value)
{
  switch (type)
    {
    case ENTRY_TYPE_STRING:
      return _py_string_from_string(value, -1);
    case ENTRY_TYPE_LONG:
      return PyLong_FromString(value, NULL, 10);
    case ENTRY_TYPE_BYTES:
      return PyBytes_FromString(value);
    default:
      g_assert_not_reached();
    }
}

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

static gboolean
load_persist_entry(PersistState *persist_state, PersistEntryHandle handle)
{
  PythonPersistMasterEntry *entry = persist_state_map_entry(persist_state, handle);
  guint8 version = entry->version;
  persist_state_unmap_entry(persist_state, handle);

  if (version != MASTER_ENTRY_VERSION)
    {
      PyErr_Format(PyExc_RuntimeError, "Invalid persist version: %d\nPossible persist file corruption", (gint)version);
      return FALSE;
    }

  return TRUE;
}

static gboolean
allocate_persist_entry(PersistState *persist_state, const gchar *persist_name)
{
  PersistEntryHandle handle = persist_state_alloc_entry(persist_state, persist_name, sizeof(PythonPersistMasterEntry));
  if (!handle)
    {
      PyErr_Format(PyExc_RuntimeError, "Could not allocate persist entry");
      return FALSE;
    }

  PythonPersistMasterEntry *entry = persist_state_map_entry(persist_state, handle);
  entry->version = MASTER_ENTRY_VERSION;
  persist_state_unmap_entry(persist_state, handle);

  return TRUE;
}

static gboolean
prepare_master_entry(PersistState *persist_state, const gchar *persist_name)
{
  gsize size;
  guint8 version;
  PersistEntryHandle handle = persist_state_lookup_entry(persist_state, persist_name, &size, &version);
  if (handle)
    return load_persist_entry(persist_state, handle);
  else
    return allocate_persist_entry(persist_state, persist_name);
}

static int
_persist_type_init(PyObject *s, PyObject *args, PyObject *kwds)
{
  PyPersist *self =(PyPersist *)s;
  const gchar *persist_name=NULL;

  self->persist_state = PyCapsule_Import("_syslogng.persist_state", FALSE);
  if (!self->persist_state)
    {
      gchar buf[256];
      _py_format_exception_text(buf, sizeof(buf));

      msg_error("Error importing persist_state",
                evt_tag_str("exception", buf));
      _py_finish_exception_handling();

      g_assert_not_reached();
    }

  static char *kwlist[] = {"persist_name", NULL};

  if (! PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &persist_name))
    return -1;

  if (g_strstr_len(persist_name, -1, SUBKEY_DELIMITER))
    {
      // Internally, we will store subkeys as persist_name##subkey
      PyErr_Format(PyExc_ValueError, "persist name cannot contain " SUBKEY_DELIMITER);
      return -1;
    }

  if (!prepare_master_entry(self->persist_state, persist_name))
    return -1;

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
  return g_strdup_printf("%s" SUBKEY_DELIMITER "%s", self->persist_name, key);
}

static gchar *
_lookup_entry(PyPersist *self, const gchar *key, guint8 *type)
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

  Entry *entry = persist_state_map_entry(self->persist_state, handle);
  *type = entry->type;
  gchar *copy = g_strdup(entry->data);
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

static gchar *
_serialize(guint8 type, PyObject *v)
{
  switch (type)
    {
    case ENTRY_TYPE_STRING:
      return g_strdup(_py_get_string_as_string(v));
    case ENTRY_TYPE_LONG:
    {
      PyObject *as_str = PyObject_Str(v);
      g_assert(as_str);
      gchar *result = g_strdup(_py_get_string_as_string(as_str));
      Py_DECREF(as_str);
      return result;
    }
    case ENTRY_TYPE_BYTES:
      return g_strdup(PyBytes_AsString(v));
    default:
      g_assert_not_reached();
    }
}

static gboolean
_store_entry(PyPersist *self, const gchar *key, guint8 type, PyObject *v)
{
  gchar *query_key = _build_key(self, key);
  gchar *value = _serialize(type, v);
  gsize value_len = strlen(value) + sizeof(type);

  PersistEntryHandle handle = _allocate_persist_entry(self->persist_state, query_key, value, value_len);
  if (!handle)
    {
      g_free(value);
      g_free(query_key);
      return FALSE;
    }

  Entry *entry = persist_state_map_entry(self->persist_state, handle);
  entry->type = type;
  strcpy(entry->data, value);
  persist_state_unmap_entry(self->persist_state, handle);

  g_free(value);
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
  guint8 type;
  gchar *value = _lookup_entry(self, name, &type);

  if (!value)
    {
      PyErr_Format(PyExc_KeyError, "No such name-value pair %s", name);
      return NULL;
    }

  if (type >= ENTRY_TYPE_MAX)
    {
      PyErr_Format(PyExc_RuntimeError, "Unknown data type: %d", (gint)type);
      g_free(value);
      return NULL;
    }

  PyObject *py_value = entry_to_pyobject(type, value);
  g_free(value);
  return py_value;
}

static int
_py_persist_type_set(PyObject *o, PyObject *k, PyObject *v)
{
  PyPersist *self = (PyPersist *)o;
  guint8 type;

  if (!_py_is_string(k))
    {
      PyErr_SetString(PyExc_TypeError, "key is not a string object");
      return -1;
    }

  if (PyBytes_Check(v))
    type = ENTRY_TYPE_BYTES;
  else if (_py_is_string(v))
    type = ENTRY_TYPE_STRING;
  else if (py_object_is_integer(v))
    type = ENTRY_TYPE_LONG;
  else
    {
      PyErr_SetString(PyExc_TypeError, "Value must be either string, integer or bytes");
      return -1;
    }

  const gchar *key = _py_get_string_as_string(k);

  if (!_store_entry(self, key, type, v))
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

static void
_insert_to_dict(gchar *key, gint entry_size, Entry *entry, gpointer *user_data)
{
  const gchar *persist_name = user_data[0];
  PyObject *entries = user_data[1];

  if (!g_str_has_prefix(key, persist_name))
    return;

  gchar *start = g_strstr_len(key, -1, SUBKEY_DELIMITER);
  if (!start)
    return;

  if (entry->type >= ENTRY_TYPE_MAX)
    return;

  PyObject *key_object = _py_string_from_string(start + strlen(SUBKEY_DELIMITER), -1);
  PyObject *value_object = entry_to_pyobject(entry->type, entry->data);
  PyDict_SetItem(entries, key_object, value_object);
  Py_XDECREF(key_object);
  Py_XDECREF(value_object);
}

PyObject *py_persist_type_iter(PyObject *o)
{
  PyPersist *self = (PyPersist *)o;

  PyObject *entries = PyDict_New();
  persist_state_foreach_entry(self->persist_state, (PersistStateForeachFunc)_insert_to_dict,
                              (gpointer [])
  {
    self->persist_name, entries
  });

  PyObject *iter = PyObject_GetIter(entries);
  Py_DECREF(entries);
  return iter;
}

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
  .tp_iter = &py_persist_type_iter,
  0,
};

void
py_persist_init(void)
{
  PyType_Ready(&py_persist_type);
  PyModule_AddObject(PyImport_AddModule("_syslogng"), "Persist", (PyObject *) &py_persist_type);
}
