/*
 * Copyright (c) 2023 Attila Szakacs
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

#ifndef _SNG_PYTHON_OPTIONS_H
#define _SNG_PYTHON_OPTIONS_H

#include "python-module.h"

typedef struct _PythonOption PythonOption;

PythonOption *python_option_string_new(const gchar *name, const gchar *value);
PythonOption *python_option_long_new(const gchar *name, gint64 value);
PythonOption *python_option_double_new(const gchar *name, gdouble value);
PythonOption *python_option_boolean_new(const gchar *name, gboolean value);
PythonOption *python_option_string_list_new(const gchar *name, const GList *value);
PythonOption *python_option_template_new(const gchar *name, const gchar *value);

PythonOption *python_option_ref(PythonOption *self);
void python_option_unref(PythonOption *self);
const gchar *python_option_get_name(const PythonOption *self);
PyObject *python_option_create_value_py_object(const PythonOption *self);

typedef struct _PythonOptions PythonOptions;

PythonOptions *python_options_new(void);
void python_options_add_option(PythonOptions *self, PythonOption *option);
PythonOptions *python_options_clone(const PythonOptions *self);
PyObject *python_options_create_py_dict(const PythonOptions *self);
void python_options_free(PythonOptions *self);

#endif
