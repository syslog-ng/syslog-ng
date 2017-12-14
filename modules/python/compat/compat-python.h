/*
 * Copyright (c) 2017 Balabit
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

#ifndef _COMPAT_PYTHON_H
#define _COMPAT_PYTHON_H

#include <Python.h>
#include "syslog-ng.h"

gboolean py_is_string(PyObject *object);
const gchar *py_object_as_string(PyObject *object);

#if (SYSLOG_NG_ENABLE_PYTHONv2)
#define PYTHON_BUILTIN_MODULE_NAME "__builtin__"
#endif

#if (SYSLOG_NG_ENABLE_PYTHONv3)
#define PYTHON_BUILTIN_MODULE_NAME "builtins"
#endif

#endif
