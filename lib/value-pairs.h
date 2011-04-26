/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2011 Gergely Nagy <algernon@balabit.hu>
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

#ifndef VALUE_PAIRS_H_INCLUDED
#define VALUE_PAIRS_H_INCLUDED 1

#include "syslog-ng.h"
#include "nvtable.h"

typedef struct _ValuePairs ValuePairs;
typedef gboolean (*VPForeachFunc)(const gchar *name, const gchar *value, gpointer user_data);

gboolean value_pairs_add_scope(ValuePairs *vp, const gchar *scope);
void value_pairs_add_exclude_glob(ValuePairs *vp, const gchar *pattern);
void value_pairs_add_pair(ValuePairs *vp, GlobalConfig *cfg, const gchar *key, const gchar *value);

void value_pairs_foreach(ValuePairs *vp, VPForeachFunc func,
                         LogMessage *msg, gint32 seq_num,
                         gpointer user_data);

ValuePairs *value_pairs_new(void);
void value_pairs_free(ValuePairs *vp);

ValuePairs *value_pairs_new_from_cmdline(GlobalConfig *cfg,
					 gint argc, gchar **argv,
					 GError **error);

#endif
