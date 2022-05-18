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
#include "compat/compat-python.h"
#include "python-helpers.h"
#include "logmsg/logmsg.h"
#include "messages.h"
#include "timeutils/cache.h"
#include "str-utils.h"
#include "timeutils/unixtime.h"
#include "timeutils/misc.h"
#include "msg-format.h"

#include <datetime.h>

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
  Py_CLEAR(self->bookmark_data);
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
  self->bookmark_data = NULL;
  return (PyObject *) self;
}

static PyObject *
py_log_message_new_empty(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
  PyObject *bookmark_data = NULL;
  const gchar *message = NULL;
  Py_ssize_t message_length = 0;

  static const gchar *kwlist[] = {"message", "bookmark", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|z#O", (gchar **) kwlist, &message, &message_length, &bookmark_data))
    return NULL;

  PyLogMessage *self = (PyLogMessage *) subtype->tp_alloc(subtype, 0);
  if (!self)
    return NULL;

  self->msg = log_msg_new_empty();
  self->bookmark_data = NULL;
  invalidate_cached_time();

  if (message)
    log_msg_set_value(self->msg, LM_V_MESSAGE, message, message_length);

  Py_XINCREF(bookmark_data);
  self->bookmark_data = bookmark_data;

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
                                  LogMessageValueType type, gpointer user_data)
{
  PyObject *list = (PyObject *)user_data;

  PyObject *py_name = PyBytes_FromString(name);
  PyList_Append(list, py_name);
  Py_XDECREF(py_name);

  return FALSE;
}

static gboolean
_is_macro_name_visible_to_user(const gchar *name, NVHandle handle)
{
  return log_msg_is_handle_macro(handle) && !_is_key_blacklisted(name);
}

static void
_collect_macro_names(gpointer key, gpointer value, gpointer user_data)
{
  const gchar *name = (const gchar *)key;
  NVHandle handle = GPOINTER_TO_UINT(value);
  PyObject *list = (PyObject *)user_data;

  if (_is_macro_name_visible_to_user(name, handle))
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
  log_msg_registry_foreach(_collect_macro_names, keys);

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
_datetime_timestamp(PyObject *py_datetime)
{
  PyObject *py_tzinfo = _py_get_attr_or_null(py_datetime, "tzinfo");
  if (!py_tzinfo)
    {
      PyErr_Format(PyExc_ValueError, "Error obtaining tzinfo");
      return NULL;
    }
  PyObject *py_epoch = PyDateTimeAPI->DateTime_FromDateAndTime(1970, 1, 1, 0, 0, 0, 0, py_tzinfo,
                       PyDateTimeAPI->DateTimeType);

  PyObject *py_delta = _py_invoke_method_by_name(py_datetime, "__sub__", py_epoch,
                                                 "PyDateTime", "py_datetime_to_logstamp");
  if (!py_delta)
    {
      Py_XDECREF(py_tzinfo);
      Py_XDECREF(py_epoch);
      PyErr_Format(PyExc_ValueError, "Error calculating POSIX timestamp");
      return NULL;
    }

  PyObject *py_posix_timestamp = _py_invoke_method_by_name(py_delta, "total_seconds", NULL,
                                                           "PyDateTime", "py_datetime_to_logstamp");
  Py_XDECREF(py_tzinfo);
  Py_XDECREF(py_delta);
  Py_XDECREF(py_epoch);

  return py_posix_timestamp;
}

static gboolean
_datetime_get_gmtoff(PyObject *py_datetime, gint *utcoffset)
{
  *utcoffset = -1;
  PyObject *py_utcoffset = _py_invoke_method_by_name(py_datetime, "utcoffset", NULL, "PyDateTime",
                                                     "py_datetime_to_logstamp");
  if (!py_utcoffset)
    {
      PyErr_Format(PyExc_ValueError, "Error obtaining timezone info");
      return FALSE;
    }

  if (py_utcoffset != Py_None)
    {
      *utcoffset = PyDateTime_DELTA_GET_SECONDS(py_utcoffset);
    }

  Py_XDECREF(py_utcoffset);
  return TRUE;
}

static gboolean
py_datetime_to_logstamp(PyObject *py_timestamp, UnixTime *logstamp)
{
  if (!PyDateTime_Check(py_timestamp))
    {
      PyErr_Format(PyExc_TypeError, "datetime expected in the first parameter");
      return FALSE;
    }

  PyObject *py_posix_timestamp = _datetime_timestamp(py_timestamp);
  if (!py_posix_timestamp)
    return FALSE;

  gdouble posix_timestamp = PyFloat_AsDouble(py_posix_timestamp);

  Py_XDECREF(py_posix_timestamp);

  gint local_gmtoff;
  if (!_datetime_get_gmtoff(py_timestamp, &local_gmtoff))
    return FALSE;
  if (local_gmtoff == -1)
    local_gmtoff = get_local_timezone_ofs((time_t) posix_timestamp);

  logstamp->ut_sec = (gint64) posix_timestamp;
  logstamp->ut_usec = (guint32) (posix_timestamp * 1000000 - logstamp->ut_sec * 1000000);
  logstamp->ut_gmtoff = local_gmtoff;

  return TRUE;
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
py_log_message_set_bookmark(PyLogMessage *self, PyObject *args, PyObject *kwrds)
{
  PyObject *bookmark_data;

  static const gchar *kwlist[] = {"bookmark", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwrds, "O", (gchar **) kwlist, &bookmark_data))
    return NULL;

  Py_CLEAR(self->bookmark_data);

  Py_XINCREF(bookmark_data);
  self->bookmark_data = bookmark_data;

  Py_RETURN_NONE;
}

static PyObject *
py_log_message_parse(PyObject *_none, PyObject *args, PyObject *kwrds)
{
  const gchar *raw_msg;
  Py_ssize_t raw_msg_length;

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

  py_msg->msg = msg_format_parse(parse_options, (const guchar *) raw_msg, raw_msg_length);
  py_msg->bookmark_data = NULL;

  return (PyObject *) py_msg;
}

static PyMethodDef py_log_message_methods[] =
{
  { "keys", (PyCFunction)_logmessage_get_keys_method, METH_NOARGS, "Return keys." },
  { "set_pri", (PyCFunction)py_log_message_set_pri, METH_VARARGS | METH_KEYWORDS, "Set priority" },
  { "set_timestamp", (PyCFunction)py_log_message_set_timestamp, METH_VARARGS | METH_KEYWORDS, "Set timestamp" },
  { "set_bookmark", (PyCFunction)py_log_message_set_bookmark, METH_VARARGS | METH_KEYWORDS, "Set bookmark" },
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
  PyDateTime_IMPORT;
  PyType_Ready(&py_log_message_type);
  PyModule_AddObject(PyImport_AddModule("_syslogng"), "LogMessage", (PyObject *) &py_log_message_type);
}
