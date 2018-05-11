/*
 * Copyright (c) 2002-2018 Balabit
 * Copyright (c) 2018 Laszlo Budai <laszlo.budai@balabit.com>
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

#ifndef TRANSPORT_FACTORY_ID_INCLUDED
#define TRANSPORT_FACTORY_ID_INCLUDED

#include "syslog-ng.h"

typedef GString TransportFactoryId;

void transport_factory_id_global_init(void);
void transport_factory_id_global_deinit(void);
void transport_factory_id_register(TransportFactoryId *);
GList *transport_factory_id_clone_registered_ids(void);
void transport_factory_id_free(gpointer);
gpointer transport_factory_id_clone(gconstpointer);
const gchar *transport_factory_id_to_string(TransportFactoryId *);

#define TRANSPORT_FACTORY_ID_NEW(name) ({GString *str = g_string_new(""); g_string_printf(str, "%s-%s:%s:%d", name, __FILE__, __func__, __LINE__); str;})
#define TRANSPORT_FACTORY_ID_FREE_FUNC transport_factory_id_free
#define TRANSPORT_FACTORY_ID_HASH_FUNC g_string_hash
#define TRANSPORT_FACTORY_ID_CMP_FUNC g_string_equal
#define TRANSPORT_FACTORY_ID_CLONE transport_factory_id_clone

#endif
