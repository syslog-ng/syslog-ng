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
#ifndef PYTHON_HELPERS_H_INCLUDED
#define PYTHON_HELPERS_H_INCLUDED 1

#include "python-module.h"

const gchar *_py_get_callable_name(PyObject *callable, gchar *buf, gsize buf_len);
const gchar *_py_format_exception_text(gchar *buf, gsize buf_len);
void _py_finish_exception_handling(void);
PyObject *_py_get_attr_or_null(PyObject *o, const gchar *attr);
PyObject *_py_do_import(const gchar *modname);
PyObject *_py_resolve_qualified_name(const gchar *name);
PyObject *_py_create_arg_dict(GHashTable *args);
PyObject *_py_invoke_function(PyObject *func, PyObject *arg, const gchar *class, const gchar *caller_context);
void _py_invoke_void_function(PyObject *func, PyObject *arg, const gchar *class, const gchar *caller_context);
gboolean _py_invoke_bool_function(PyObject *func, PyObject *arg, const gchar *class, const gchar *caller_context);
PyObject *_py_get_method(PyObject *instance, const gchar *method_name, const gchar *module);
void _py_invoke_void_method_by_name(PyObject *instance, const gchar *method_name, const gchar *class,
                                    const gchar *module);
gboolean _py_invoke_bool_method_by_name_with_args(PyObject *instance, const gchar *method_name, GHashTable *args,
                                                  const gchar *class, const gchar *module);
gboolean _py_invoke_bool_method_by_name(PyObject *instance, const gchar *method_name, const gchar *class,
                                        const gchar *module);
void _py_perform_imports(GList *imports);
gboolean _py_is_string(PyObject *object);
const gchar *_py_get_string_as_string(PyObject *object);
PyObject *_py_string_from_string(const gchar *str, gssize len);

#endif
