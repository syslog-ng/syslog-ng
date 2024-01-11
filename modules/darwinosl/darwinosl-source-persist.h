/*
 * Copyright (c) 2020 Attila Szakacs <attila.szakacs@balabit.com>
 * Copyright (c) 2023 Hofi <hofione@gmail.com>
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

#ifndef DARWINOSL_SOURCE_PERSIST_H
#define DARWINOSL_SOURCE_PERSIST_H

#include "syslog-ng.h"
#include "persist-state.h"

typedef struct _DarwinOSLogSourcePersist DarwinOSLogSourcePersist;
typedef struct _DarwinOSLogSourcePosition
{
  gdouble log_position;
  guint last_msg_hash;
  guint last_used_filter_predicate_hash;

} DarwinOSLogSourcePosition;

DarwinOSLogSourcePersist *darwinosl_source_persist_new(void);
void darwinosl_source_persist_free(DarwinOSLogSourcePersist *self);
gboolean darwinosl_source_persist_init(DarwinOSLogSourcePersist *self,
                                       PersistState *state,
                                       const gchar *persist_name);

void darwinosl_source_persist_fill_bookmark(DarwinOSLogSourcePersist *self, Bookmark *bookmark,
                                            DarwinOSLogSourcePosition position);
void darwinosl_source_persist_load_position(DarwinOSLogSourcePersist *self,
                                            DarwinOSLogSourcePosition *position);

#endif
