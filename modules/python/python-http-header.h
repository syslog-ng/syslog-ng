/*
 * Copyright (c) 2020 One Identity
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

#ifndef _SNG_PYTHON_AUTH_HEADERS_H
#define _SNG_PYTHON_AUTH_HEADERS_H

#include "python-module.h"

typedef struct _PythonHttpHeaderPlugin PythonHttpHeaderPlugin;

PythonHttpHeaderPlugin *python_http_header_new(void);

void python_http_header_set_loaders(PythonHttpHeaderPlugin *self, GList *loaders);
void python_http_header_set_class(PythonHttpHeaderPlugin *self, gchar *class);
void python_http_header_set_option(PythonHttpHeaderPlugin *self, gchar *key, gchar *value);
void python_http_header_set_mark_errors_as_critical(PythonHttpHeaderPlugin *self, gboolean enable);

#endif
