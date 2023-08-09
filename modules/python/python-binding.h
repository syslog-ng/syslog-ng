/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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

#ifndef PYTHON_BINDING_H_INCLUDED
#define PYTHON_BINDING_H_INCLUDED 1

#include "python-options.h"

typedef struct _PythonBinding
{
  gchar *class;
  GList *loaders;

  PythonOptions *options;
} PythonBinding;

void python_binding_set_loaders(PythonBinding *self, GList *loaders);
void python_binding_set_class(PythonBinding *self, gchar *class);

gboolean python_binding_init(PythonBinding *self, GlobalConfig *cfg, const gchar *desc);
void python_binding_deinit(PythonBinding *self);

void python_binding_clone(PythonBinding *self, PythonBinding *cloned);
void python_binding_init_instance(PythonBinding *self);
void python_binding_clear(PythonBinding *self);

#endif
