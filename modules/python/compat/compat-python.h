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

#if SYSLOG_NG_ENABLE_PYTHONv2
#define PYTHON_BUILTIN_MODULE_NAME "__builtin__"
#define PYTHON_MODULE_VERSION "python2"

/*
 * Macro from python3 Include/datetime.h file
 */
#define PyDateTime_DELTA_GET_SECONDS(o)      (((PyDateTime_Delta*)o)->seconds)
#endif

#if SYSLOG_NG_ENABLE_PYTHONv3
#define PYTHON_BUILTIN_MODULE_NAME "builtins"
#define PYTHON_MODULE_VERSION "python3"
#endif

void py_init_argv(void);
PyObject *int_as_pyobject(gint num);

gint pyobject_as_int(PyObject *object);
#endif
