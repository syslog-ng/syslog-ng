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

/* we have a GlobalConfig specific main module, which gets switched as
 * configurations are initialized or deinitialized.  This main module hosts
 * the code in global python {} blocks.  Also, some functionality (e.g.
 * Persist or LogTemplate) relate to a GlobalConfig instance */

gboolean _py_init_main_module_for_config(PythonConfig *pc);
void _py_switch_to_config_main_module(PythonConfig *pc);
PythonConfig *_py_get_config_from_main_module(void);

PyObject *_py_get_main_module(PythonConfig *pc);

#endif
