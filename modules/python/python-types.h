/*
 * Copyright (c) 2022 One Identity
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
#ifndef PYTHON_TYPES_H_INCLUDED
#define PYTHON_TYPES_H_INCLUDED 1

#include "python-module.h"
#include "logmsg/type-hinting.h"
#include "template/templates.h"

PyObject *py_bytes_from_string(const char *value, gssize len);
PyObject *py_string_from_string(const char *value, gssize len);
PyObject *py_long_from_long(gint64 l);
PyObject *py_double_from_double(gdouble d);
PyObject *py_boolean_from_boolean(gboolean b);
PyObject *py_list_from_list(const gchar *list, gssize list_len);
PyObject *py_string_list_from_string_list(const GList *string_list);
PyObject *py_datetime_from_unix_time(UnixTime *ut);
PyObject *py_datetime_from_msec(gint64 msec);
PyObject *py_obj_from_log_msg_value(const gchar *value, gssize value_len, LogMessageValueType type);

gboolean is_py_obj_bytes_or_string_type(PyObject *obj);

gboolean py_bytes_or_string_to_string(PyObject *obj, const gchar **string);
gboolean py_long_to_long(PyObject *obj, gint64 *l);
gboolean py_double_to_double(PyObject *obj, gdouble *d);
gboolean py_boolean_to_boolean(PyObject *obj, gboolean *b);
gboolean py_list_to_list(PyObject *obj, GString *list);
gboolean py_string_list_to_string_list(PyObject *obj, GList **string_list);
gboolean py_datetime_to_unix_time(PyObject *obj, UnixTime *ut);
gboolean py_datetime_to_datetime(PyObject *obj, GString *dt);
gboolean py_obj_to_log_msg_value(PyObject *obj, GString *value, LogMessageValueType *type);


void py_init_types(void);

#endif
