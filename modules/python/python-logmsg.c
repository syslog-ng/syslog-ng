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
#include "python-types.h"
#include "python-main.h"
#include "logmsg/logmsg.h"
#include "messages.h"
#include "timeutils/cache.h"
#include "str-utils.h"
#include "timeutils/unixtime.h"
#include "timeutils/misc.h"
#include "msg-format.h"
#include "scratch-buffers.h"
#include "cfg.h"
#include "gsockaddr.h"

#include <datetime.h>

int
py_is_log_message(PyObject *obj)
{
  return PyType_IsSubtype(Py_TYPE(obj), &py_log_message_type);
}

static inline PyObject *
_get_value(PyLogMessage *self, const gchar *name, gboolean cast_to_bytes, gboolean *error)
{
  *error = FALSE;
  NVHandle handle = log_msg_get_value_handle(name);
  gssize value_len = 0;
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_if_set_with_type(self->msg, handle, &value_len, &type);

  if (!value || type == LM_VT_BYTES || type == LM_VT_PROTOBUF)
    return NULL;

  if (cast_to_bytes)
    type = LM_VT_STRING;

  APPEND_ZERO(value, value, value_len);

  PyObject *py_value = py_obj_from_log_msg_value(value, value_len, type);
  if (!py_value)
    {
      PyErr_Format(PyExc_TypeError, "Error converting a name-value (%s) pair to a Python object", name);
      *error = TRUE;
      return NULL;
    }

  return py_value;
}

static PyObject *
_py_log_message_subscript(PyObject *o, PyObject *key)
{
  const gchar *name;
  if (!py_bytes_or_string_to_string(key, &name))
    {
      PyErr_SetString(PyExc_TypeError, "key is not a string object");
      return NULL;
    }

  PyLogMessage *py_msg = (PyLogMessage *) o;
  gboolean error;
  PyObject *value = _get_value(py_msg, name, py_msg->cast_to_bytes, &error);

  if (error)
    return NULL;

  if (value)
    return value;

  /* compat mode (3.x), we don't raise KeyError */
  if (py_msg->cast_to_bytes)
    return py_bytes_from_string("", -1);

  PyErr_Format(PyExc_KeyError, "No such name-value pair %s", name);
  return NULL;
}

static int
_py_log_message_ass_subscript(PyObject *o, PyObject *key, PyObject *value)
{
  const gchar *name;
  if (!py_bytes_or_string_to_string(key, &name))
    {
      PyErr_SetString(PyExc_TypeError, "key is not a string object");
      return -1;
    }

  PyLogMessage *py_msg = (PyLogMessage *) o;
  LogMessage *msg = py_msg->msg;

  if (log_msg_is_write_protected(msg))
    {
      PyErr_Format(PyExc_TypeError,
                   "Log message is read only, cannot set name-value pair %s, "
                   "you are possibly trying to change a LogMessage from a "
                   "destination driver,  which is not allowed", name);
      return -1;
    }

  NVHandle handle = log_msg_get_value_handle(name);

  if (!value)
    return -1;

  if (py_msg->cast_to_bytes && !is_py_obj_bytes_or_string_type(value))
    {
      PyErr_Format(PyExc_ValueError,
                   "str or bytes object expected as log message values, got type %s (key %s). "
                   "Earlier syslog-ng accepted any type, implicitly converting it to a string. "
                   "Later syslog-ng (at least 4.0) will store the value with the correct type. "
                   "With this version please convert it explicitly to string/bytes",
                   value->ob_type->tp_name, name);
      return -1;
    }

  ScratchBuffersMarker marker;
  GString *log_msg_value = scratch_buffers_alloc_and_mark(&marker);
  LogMessageValueType type;

  if (!py_obj_to_log_msg_value(value, log_msg_value, &type))
    {
      scratch_buffers_reclaim_marked(marker);
      return -1;
    }

  log_msg_set_value_with_type(py_msg->msg, handle, log_msg_value->str, -1, type);

  scratch_buffers_reclaim_marked(marker);
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
py_log_message_new(LogMessage *msg, GlobalConfig *cfg)
{
  PyLogMessage *self;

  self = PyObject_New(PyLogMessage, &py_log_message_type);
  if (!self)
    return NULL;

  self->msg = log_msg_ref(msg);
  self->bookmark_data = NULL;

  if (cfg_is_config_version_older(cfg, VERSION_VALUE_4_0))
    self->cast_to_bytes = TRUE;
  else
    self->cast_to_bytes = FALSE;
  return (PyObject *) self;
}

static int
py_log_message_init(PyObject *s, PyObject *args, PyObject *kwds)
{
  PyLogMessage *self = (PyLogMessage *) s;
  PyObject *bookmark_data = NULL;
  const gchar *message = NULL;
  Py_ssize_t message_length = 0;

  static const gchar *kwlist[] = {"message", "bookmark", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|z#O", (gchar **) kwlist, &message, &message_length, &bookmark_data))
    return -1;

  self->msg = log_msg_new_empty();
  self->bookmark_data = NULL;
  invalidate_cached_realtime();

  if (message)
    log_msg_set_value(self->msg, LM_V_MESSAGE, message, message_length);

  Py_XINCREF(bookmark_data);
  self->bookmark_data = bookmark_data;

  return 0;
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

  if (type == LM_VT_BYTES || type == LM_VT_PROTOBUF)
    return FALSE;

  PyObject *py_name = PyBytes_FromString(name);
  PyList_Append(list, py_name);
  Py_XDECREF(py_name);

  return FALSE;
}

static gboolean
_is_macro_name_visible_to_user(const gchar *name, NVHandle handle)
{
  return log_msg_is_handle_macro(handle);
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
py_log_message_get(PyLogMessage *self, PyObject *args, PyObject *kwrds)
{
  const gchar *key = NULL;
  Py_ssize_t key_len = 0;
  PyObject *default_value = NULL;

  static const gchar *kwlist[] = {"key", "default", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwrds, "z#|O", (gchar **) kwlist, &key, &key_len, &default_value))
    return NULL;

  gboolean error;
  PyObject *value = _get_value(self, key, self->cast_to_bytes, &error);

  if (error)
    return NULL;

  if (value)
    return value;

  if (!default_value)
    Py_RETURN_NONE;

  Py_XINCREF(default_value);
  return default_value;
}

static PyObject *
py_log_message_get_as_str(PyLogMessage *self, PyObject *args, PyObject *kwrds)
{
  const gchar *key = NULL;
  Py_ssize_t key_len = 0;
  PyObject *default_value = NULL;
  const gchar *encoding = "utf-8";
  const gchar *errors = "strict";
  const gchar *repr = "internal";

  static const gchar *kwlist[] = {"key", "default", "encoding", "errors", "repr", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwrds, "z#|Osss", (gchar **) kwlist, &key, &key_len, &default_value,
                                   &encoding, &errors, &repr))
    {
      return NULL;
    }

  NVHandle handle = log_msg_get_value_handle(key);
  gssize value_len = 0;
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_if_set_with_type(self->msg, handle, &value_len, &type);

  if (value && type != LM_VT_BYTES && type != LM_VT_PROTOBUF)
    {
      APPEND_ZERO(value, value, value_len);
      return PyUnicode_Decode(value, value_len, encoding, errors);
    }

  if (!default_value)
    Py_RETURN_NONE;

  if (!PyUnicode_Check(default_value) && default_value != Py_None)
    {
      PyErr_Format(PyExc_TypeError, "default is not a string object");
      return NULL;
    }

  Py_XINCREF(default_value);
  return default_value;
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
py_log_message_get_pri(PyLogMessage *self, PyObject *args, PyObject *kwrds)
{
  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  return PyLong_FromLong(self->msg->pri);
}

static PyObject *
py_log_message_set_source_ipaddress(PyLogMessage *self, PyObject *args, PyObject *kwrds)
{
  const gchar *ip;
  Py_ssize_t ip_length;
  guint port = 0;

  static const gchar *kwlist[] = {"ip", "port", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwrds, "z#|I", (gchar **) kwlist, &ip, &ip_length, &port))
    return NULL;

  if (!ip)
    Py_RETURN_FALSE;

  GSockAddr *saddr = g_sockaddr_inet_or_inet6_new(ip, (guint16) port);
  if (!saddr)
    Py_RETURN_FALSE;

  log_msg_set_saddr(self->msg, saddr);
  Py_RETURN_TRUE;
}

static PyObject *
py_log_message_set_recvd_rawmsg_size(PyLogMessage *self, PyObject *args, PyObject *kwrds)
{
  gulong rawmsg_size;

  static const gchar *kwlist[] = {"rawmsg_size", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwrds, "k", (gchar **) kwlist, &rawmsg_size))
    return NULL;

  log_msg_set_recvd_rawmsg_size(self->msg, rawmsg_size);

  Py_RETURN_NONE;
}

static PyObject *
py_log_message_set_timestamp(PyLogMessage *self, PyObject *args, PyObject *kwrds)
{
  PyObject *py_timestamp;

  static const gchar *kwlist[] = {"timestamp", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwrds, "O", (gchar **) kwlist, &py_timestamp))
    return NULL;

  if (!py_datetime_to_unix_time((PyObject *) py_timestamp, &self->msg->timestamps[LM_TS_STAMP]))
    {
      PyErr_Format(PyExc_ValueError, "Error extracting timestamp from value, expected a datetime instance");
      return NULL;
    }

  Py_RETURN_NONE;
}

static PyObject *
py_log_message_get_timestamp(PyLogMessage *self, PyObject *args, PyObject *kwrds)
{
  if (!PyArg_ParseTuple(args, ""))
    return NULL;

  return py_datetime_from_unix_time(&self->msg->timestamps[LM_TS_STAMP]);
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
  { "get", (PyCFunction)py_log_message_get, METH_VARARGS | METH_KEYWORDS, "Get value" },
  { "get_as_str", (PyCFunction)py_log_message_get_as_str, METH_VARARGS | METH_KEYWORDS, "Get value as string" },
  { "set_pri", (PyCFunction)py_log_message_set_pri, METH_VARARGS | METH_KEYWORDS, "Set syslog priority" },
  { "get_pri", (PyCFunction)py_log_message_get_pri, METH_VARARGS | METH_KEYWORDS, "Get syslog priority" },
  { "set_source_ipaddress", (PyCFunction)py_log_message_set_source_ipaddress, METH_VARARGS | METH_KEYWORDS, "Set source address" },
  { "set_recvd_rawmsg_size", (PyCFunction)py_log_message_set_recvd_rawmsg_size, METH_VARARGS | METH_KEYWORDS, "Set raw message size" },
  { "set_timestamp", (PyCFunction)py_log_message_set_timestamp, METH_VARARGS | METH_KEYWORDS, "Set timestamp" },
  { "get_timestamp", (PyCFunction)py_log_message_get_timestamp, METH_VARARGS | METH_KEYWORDS, "Get timestamp" },
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
  .tp_new = PyType_GenericNew,
  .tp_init = py_log_message_init,
  .tp_as_mapping = &py_log_message_mapping,
  .tp_methods = py_log_message_methods,
  0,
};

void
py_log_message_global_init(void)
{
  PyDateTime_IMPORT;
  PyType_Ready(&py_log_message_type);
  PyModule_AddObject(PyImport_AddModule("_syslogng"), "LogMessage", (PyObject *) &py_log_message_type);
}
