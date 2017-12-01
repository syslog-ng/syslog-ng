/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#ifndef CFG_ARGS_H_INCLUDED
#define CFG_ARGS_H_INCLUDED 1

#include "syslog-ng.h"

typedef struct _CfgArgs CfgArgs;

/* argument list for a block generator */
gboolean cfg_args_validate(CfgArgs *self, CfgArgs *defs, const gchar *context);
gchar *cfg_args_format_varargs(CfgArgs *self, CfgArgs *defaults);
void cfg_args_set(CfgArgs *self, const gchar *name, const gchar *value);
const gchar *cfg_args_get(CfgArgs *self, const gchar *name);
void cfg_args_foreach(CfgArgs *self, GHFunc func, gpointer user_data);

CfgArgs *cfg_args_new(void);
CfgArgs *cfg_args_ref(CfgArgs *self);
void cfg_args_unref(CfgArgs *self);

#endif
