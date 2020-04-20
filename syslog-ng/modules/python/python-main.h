/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
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
#ifndef PYTHON_MAIN_H_INCLUDED
#define PYTHON_MAIN_H_INCLUDED 1

#include "python-config.h"
#include "cfg-lexer.h"

PyObject *_py_get_current_main_module(void);
PyObject *_py_get_main_module(PythonConfig *pc);
void _py_switch_main_module(PythonConfig *pc);
gboolean python_evaluate_global_code(GlobalConfig *cfg, const gchar *code, YYLTYPE *yylloc);


#endif
