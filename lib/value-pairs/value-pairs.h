/*
 * Copyright (c) 2011-2014 Balabit
 * Copyright (c) 2011-2014 Gergely Nagy <algernon@balabit.hu>
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
#include "logmsg/nvtable.h"
#include "value-pairs/transforms.h"
#include "type-hinting.h"
#include "template/templates.h"
#include "atomic.h"

typedef struct _ValuePairs ValuePairs;

typedef gboolean
(*VPForeachFunc) (const gchar *name, TypeHint type, const gchar *value,
                  gsize value_len, gpointer user_data);

typedef gboolean
(*VPWalkValueCallbackFunc) (const gchar *name, const gchar *prefix,
                            TypeHint type, const gchar *value, gsize value_len,
                            gpointer *prefix_data, gpointer user_data);
typedef gboolean (*VPWalkCallbackFunc)(const gchar *name,
                                       const gchar *prefix, gpointer *prefix_data,
                                       const gchar *prev, gpointer *prev_data,
                                       gpointer user_data);

gboolean value_pairs_add_scope(ValuePairs *vp, const gchar *scope);
void value_pairs_add_glob_pattern(ValuePairs *vp, const gchar *pattern, gboolean include);
void value_pairs_add_glob_patterns(ValuePairs *vp, GList *patterns, gboolean include);
void value_pairs_add_pair(ValuePairs *vp, const gchar *key, LogTemplate *value);

void value_pairs_add_transforms(ValuePairs *vp, ValuePairsTransformSet *vpts);

gboolean value_pairs_foreach_sorted(ValuePairs *vp, VPForeachFunc func,
                                    GCompareFunc compare_func,
                                    LogMessage *msg, gint32 seq_num, gint tz,
                                    const LogTemplateOptions *template_options,
                                    gpointer user_data);
gboolean value_pairs_foreach(ValuePairs *vp, VPForeachFunc func,
                             LogMessage *msg, gint32 seq_num, gint tz,
                             const LogTemplateOptions *template_options,
                             gpointer user_data);

gboolean value_pairs_walk(ValuePairs *vp,
                          VPWalkCallbackFunc obj_start_func,
                          VPWalkValueCallbackFunc process_value_func,
                          VPWalkCallbackFunc obj_end_func,
                          LogMessage *msg, gint32 seq_num, gint tz,
                          const LogTemplateOptions *template_options,
                          gpointer user_data);

ValuePairs *value_pairs_new(void);
ValuePairs *value_pairs_new_default(GlobalConfig *cfg);
ValuePairs *value_pairs_ref(ValuePairs *self);
void value_pairs_unref(ValuePairs *self);

void value_pairs_global_init(void);
void value_pairs_global_deinit(void);


#endif
