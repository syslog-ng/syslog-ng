/*
 * Copyright (c) 2016 Balabit
 * Copyright (c) 2016 Laszlo Budai
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


#ifndef RESOLVED_CONFIGURABLE_PATHS_H_INCLUDED
#define RESOLVED_CONFIGURABLE_PATHS_H_INCLUDED

#include <glib.h>

typedef struct _ResolvedConfigurablePaths
{
  const gchar *cfgfilename;
  const gchar *persist_file;
  const gchar *ctlfilename;
  const gchar *initial_module_path;
} ResolvedConfigurablePaths;

extern ResolvedConfigurablePaths resolved_configurable_paths;
void resolved_configurable_paths_init(ResolvedConfigurablePaths *self);

#endif
