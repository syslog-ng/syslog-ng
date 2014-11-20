/*
 * Copyright (c) 2014 BalaBit S.a.r.l., Luxembourg, Luxembourg
 * Copyright (c) 2014 Laszlo Budai
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

#ifndef HOST_ID_H
#define HOST_ID_H
#include "persist-state.h"
#include "persistable-state-header.h"

#define HOST_ID_PERSIST_KEY "host_id"

typedef struct _HostIdState
{
  PersistableStateHeader header;
  guint32 host_id;
} HostIdState;

void host_id_init(PersistState *state);
void host_id_deinit(void);
guint32 host_id_get(void);
void host_id_append_formatted_id(GString* str, guint32 id);

#endif
