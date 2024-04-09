/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#ifndef FILTERX_SCOPE_H_INCLUDED
#define FILTERX_SCOPE_H_INCLUDED

#include "filterx-object.h"
#include "logmsg/logmsg.h"

typedef struct _FilterXVariable FilterXVariable;

FilterXObject *filterx_variable_get_value(FilterXVariable *v);
void filterx_variable_set_value(FilterXVariable *v, FilterXObject *new_value);
void filterx_variable_unset_value(FilterXVariable *v);
gboolean filterx_variable_is_set(FilterXVariable *v);


/*
 * FilterXScope represents variables in a filterx scope.
 *
 * Variables are either tied to a LogMessage (when we are caching
 * demarshalled values in the scope) or are values that are "floating", e.g.
 * not yet tied to any values in the underlying LogMessage.
 *
 * Floating values are "temp" values that are not synced to the LogMessage
 * upon the exit from the scope.
 *
 */
typedef struct _FilterXScope FilterXScope;

void filterx_scope_sync_to_message(FilterXScope *self, LogMessage *msg);

FilterXVariable *filterx_scope_lookup_variable(FilterXScope *self, NVHandle handle);
FilterXVariable *filterx_scope_register_variable(FilterXScope *self,
                                                 NVHandle handle, gboolean floating,
                                                 FilterXObject *initial_value);

void filterx_scope_store_weak_ref(FilterXScope *self, FilterXObject *object);


/* copy on write */
void filterx_scope_write_protect(FilterXScope *self);
FilterXScope *filterx_scope_make_writable(FilterXScope **pself);

FilterXScope *filterx_scope_new(void);
FilterXScope *filterx_scope_ref(FilterXScope *self);
void filterx_scope_unref(FilterXScope *self);

#endif