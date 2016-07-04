/*
 * Copyright (c) 2016 Balabit
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

#ifndef KVTAGDB_H_INCLUDED
#define KVTAGDB_H_INCLUDED

#include "syslog-ng.h"
#include "tagrecord-scanner.h"
#include <stdio.h>

typedef struct _KVTagDB KVTagDB;

typedef void(*KVTAGDB_TAG_CB)(gpointer arg, const TagRecord *record);

KVTagDB* kvtagdb_new();
void kvtagdb_free(KVTagDB *self);

KVTagDB* kvtagdb_ref(KVTagDB *self);
void kvtagdb_unref(KVTagDB *self);

void kvtagdb_purge(KVTagDB *self);
void kvtagdb_index(KVTagDB *self);
gboolean kvtagdb_is_loaded(KVTagDB *self);
gboolean kvtagdb_is_indexed(KVTagDB *self);

void kvtagdb_insert(KVTagDB *self, const TagRecord *record);
gboolean kvtagdb_contains(KVTagDB *self, const gchar *selector);
gsize kvtagdb_number_of_tags(KVTagDB *self, const gchar *selector);
void kvtagdb_foreach_tag(KVTagDB *self, const gchar *selector, KVTAGDB_TAG_CB callback, gpointer arg);

GList *kvtagdb_get_selectors(KVTagDB *self);

gboolean kvtagdb_import(KVTagDB *self, FILE *fp, TagRecordScanner *scanner);

#endif
