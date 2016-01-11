/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 2013 Viktor Juhasz
 * Copyright (c) 2013 Viktor Tusa
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
#ifndef _RCPTID_H
#define _RCPTID_H

#include "syslog-ng.h"
#include "persist-state.h"
#include "persistable-state-header.h"

typedef struct _RcptidState
{
  PersistableStateHeader header;
  guint64 g_rcptid;
} RcptidState;

void rcptid_set_id(guint64 id);
guint64 rcptid_generate_id(void);
void rcptid_append_formatted_id(GString *result, guint64 rcptid);

gboolean rcptid_init(PersistState *state, gboolean use_rcptid);
void rcptid_deinit(void);

#endif
