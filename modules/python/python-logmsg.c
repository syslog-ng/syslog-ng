/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "python-logmsg.h"
#include "python-helpers.h"
#include "logmsg/logmsg.h"
#include "messages.h"
#include "str-utils.h"
#include "logstamp.h"

int
py_is_log_message(PyObject *obj)
{
  return PyType_IsSubtype(Py_TYPE(obj), &py_log_message_type);
}

static int
_str_cmp(const void *s1, const void *s2)
{
  return strcmp(*(const gchar **)s1, *(const gchar **)s2);
}

static void
_populate_blacklisted_keys(const gchar ***blacklist, size_t *n)
{
  static const gchar *keys[] =
  {
    "S_STAMP", "_", "C_STAMP"
  };
  static gboolean keys_sorted = FALSE;
  if (!keys_sorted)
    {
      keys_sorted = TRUE;
      qsort(&keys[0], sizeof(keys)/sizeof(keys[0]), sizeof(gchar *), _str_cmp);
    }
  *blacklist = keys;
  *n = sizeof(keys)/sizeof(keys[0]);
}

static gboolean
_is_key_blacklisted(const gchar *key)
{
  const gchar **blacklist = NULL;
  size_t n = 0;
  _populate_blacklisted_keys(&blacklist, &n);
  return (bsearch(&key, blacklist, n, sizeof(gchar *), _str_cmp) != NULL);
}

static PyObject *
_py_log_message_subscript(PyObject *o, PyObject *key)
{
  if (!_py_is_string(key))
    {
      PyErr_SetString(PyExc_TypeError, "key is not a string object");
      return NULL;
    }

  const gchar *name = _py_get_string_as_string(key);

  if (_is_key_blacklisted(name))
    {
      PyErr_Format(PyExc_KeyError, "Blacklisted attribute %s was requested", name);
      return NULL;
    }
  NVHandle handle = log_msg_get_value_handle(name);
  PyLogMessage *py_msg = (PyLogMessage *)o;
  gssize value_len = 0;
  const gchar *value = log_msg_get_value(py_msg->msg, handle, &value_len);

  if (!value)
    {
      PyErr_Format(PyExc_KeyError, "No such name-value pair %s", name);
      return NULL;
    }

  APPEND_ZERO(value, value, value_len);

  return PyBytes_FromString(value);
}

static int
_py_log_message_ass_subscript(PyObject *o, PyObject *key, PyObject *value)
{
  if (!_py_is_string(key))
    {
      PyErr_SetString(PyExc_TypeError, "key is not a string object");
      return -1;
    }

  PyLogMessage *py_msg = (PyLogMessage *) o;
  LogMessage *msg = py_msg->msg;
  const gchar *name = _py_get_string_as_string(key);

  if (log_msg_is_write_protected(msg))
    {
      PyErr_Format(PyExc_TypeError,
                   "Log message is read only, cannot set name-value pair %s, "
                   "you are possibly trying to change a LogMessage from a "
                   "destination driver,  which is not allowed", name);
      return -1;
    }

  NVHandle handle = log_msg_get_value_handle(name);

  if (value && _py_is_string(value))
    {
      log_msg_set_value(py_msg->msg, handle, _py_get_string_as_string(value), -1);
    }
  else
    {
      PyErr_Format(PyExc_ValueError,
                   "str or unicode object expected as log message values, got type %s (key %s). "
                   "Earlier syslog-ng accepted any type, implicitly converting it to a string. "
                   "With this version please convert it explicitly to a string using str()",
                   value->ob_type->tp_name, name);
      return -1;
    }

  return 0;
}

static void
py_log_message_free(PyLogMessage *self)
{
  log_msg_unref(self->msg);
  Py_TYPE(self)->tp_free((PyObject *) self);
}

PyObject *
py_log_message_new(LogMessage *msg)
{
  PyLogMessage *self;

  self = PyObject_New(PyLogMessage, &py_log_message_type);
  if (!self)
    return NULL;

  self->msg = log_msg_ref(msg);
  return (PyObject *) self;
}

static PyObject *
py_log_message_new_empty(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
  const gchar *message = NULL;
  gint message_length = 0;

  static const gchar *kwlist[] = {"message", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|z#", (gchar **) kwlist, &message, &message_length))
    return NULL;

  PyLogMessage *self = (PyLogMessage *) subtype->tp_alloc(subtype, 0);
  if (!self)
    return NULL;

  self->msg = log_msg_new_empty();
  invalidate_cached_time();

  if (message)
    log_msg_set_value(self->msg, LM_V_MESSAGE, message, message_length);

  return (PyObject *) self;
}

static PyMappingMethods py_log_message_mapping =
{
  .mp_length = NULL,
  .mp_subscript = (binaryfunc) _py_log_message_subscript,
  .mp_ass_subscript = (objobjargproc) _py_log_message_ass_subscript
};

static gboolean
_collect_nvpair_names_from_logmsg(NVHandle handle, const gchar *name, const gchar *value, gssize value_len,
                                  gpointer user_data)
{
  PyObject *list = (PyObject *)user_data;

  PyObject *py_name = PyBytes_FromString(name);
  PyList_Append(list, py_name);
  Py_XDECREF(py_name);

  return FALSE;
}

static gboolean
_is_key_assigned_to_match_handle(const gchar *s)
{
  char *end = NULL;
  long val = strtol(s, &end, 10);

  if (*end == '\0' && (val >= 0 && val <= 255))
    return TRUE;

  return FALSE;
}

static gboolean
_is_macro_name_visible_to_user(LogMessage *logmsg, const gchar *name)
{
  gssize value_len;

  return (!_is_key_blacklisted(name) &&
          (!_is_key_assigned_to_match_handle(name) ||
           (_is_key_assigned_to_match_handle(name) &&
            log_msg_get_value_by_name(logmsg, name, &value_len) != NULL
            && value_len > 0)));
}

static void
_collect_macro_names(gpointer key, gpointer value, gpointer user_data)
{
  gpointer *args = (gpointer *)user_data;
  LogMessage *logmsg = (LogMessage *)args[0];
  PyObject *list = (PyObject *)args[1];
  const gchar *name = (const gchar *)key;

  if (_is_macro_name_visible_to_user(logmsg, name))
    {
      PyObject *py_name = PyBytes_FromString(name);
      PyList_Append(list, py_name);
      Py_XDECREF(py_name);
    }
}

static PyObject *
_logmessage_get_keys_method(PyLogMessage *self)
{
  PyObject *keys = PyList_New(0);
  LogMessage *msg = self->msg;

  log_msg_values_foreach(msg, _collect_nvpair_names_from_logmsg, (gpointer) keys);
  gpointer registry_foreach_args[] = { msg, keys };
  log_msg_registry_foreach(_collect_macro_names, (gpointer) registry_foreach_args);

  return keys;
}

static PyObject *
py_log_message_set_pri(PyLogMessage *self, PyObject *args, PyObject *kwrds)
{
  guint pri;

  static const gchar *kwlist[] = {"pri", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwrds, "I", (gchar **) kwlist, &pri))
    return NULL;

  self->msg->pri = pri;

  Py_RETURN_NONE;
}

static PyObject *
py_log_message_set_timestamp(PyLogMessage *self, PyObject *args, PyObject *kwrds)
{
  PyObject *py_timestamp;

  static const gchar *kwlist[] = {"timestamp", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwrds, "O", (gchar **) kwlist, &py_timestamp))
    return NULL;

  if (!py_datetime_to_logstamp((PyObject *) py_timestamp, &self->msg->timestamps[LM_TS_STAMP]))
    return NULL;

  Py_RETURN_NONE;
}

static PyObject *
py_log_message_parse(PyObject *_none, PyObject *args, PyObject *kwrds)
{
  const gchar *raw_msg;
  gint raw_msg_length;

  PyObject *py_parse_options;

  static const gchar *kwlist[] = {"raw_msg", "parse_options", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwrds, "s#O", (gchar **) kwlist, &raw_msg, &raw_msg_length, &py_parse_options))
    return NULL;

  if (!PyCapsule_CheckExact(py_parse_options))
    {
      PyErr_Format(PyExc_TypeError, "Parse options (PyCapsule) expected in the second parameter");
      return NULL;
    }

  MsgFormatOptions *parse_options = PyCapsule_GetPointer(py_parse_options, NULL);
  if (!parse_options)
    {
      PyErr_Clear();
      PyErr_Format(PyExc_TypeError, "Invalid parse options (PyCapsule)");
      return NULL;
    }

  PyLogMessage *py_msg = PyObject_New(PyLogMessage, &py_log_message_type);
  if (!py_msg)
    {
      PyErr_Format(PyExc_TypeError, "Error creating new PyLogMessage");
      return NULL;
    }

  py_msg->msg = log_msg_new(raw_msg, raw_msg_length, NULL, parse_options);

  return (PyObject *) py_msg;
}

static PyMethodDef py_log_message_methods[] =
{
  { "keys", (PyCFunction)_logmessage_get_keys_method, METH_NOARGS, "Return keys." },
  { "set_pri", (PyCFunction)py_log_message_set_pri, METH_VARARGS | METH_KEYWORDS, "Set priority" },
  { "set_timestamp", (PyCFunction)py_log_message_set_timestamp, METH_VARARGS | METH_KEYWORDS, "Set timestamp" },
  { "parse", (PyCFunction)py_log_message_parse, METH_STATIC|METH_VARARGS|METH_KEYWORDS, "Parse and create LogMessage" },
  {NULL}
};

PyTypeObject py_log_message_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "LogMessage",
  .tp_basicsize = sizeof(PyLogMessage),
  .tp_dealloc = (destructor) py_log_message_free,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "LogMessage class encapsulating a syslog-ng log message",
  .tp_new = py_log_message_new_empty,
  .tp_as_mapping = &py_log_message_mapping,
  .tp_methods = py_log_message_methods,
  0,
};

void
py_log_message_init(void)
{
  PyType_Ready(&py_log_message_type);
  PyModule_AddObject(PyImport_AddModule("syslogng"), "LogMessage", (PyObject *) &py_log_message_type);
}
