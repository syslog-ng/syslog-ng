/*
 * Copyright (c) 2017 Balabit
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
#ifndef MODULES_AFFILE_COLLECTION_COMPARATOR_H_
#define MODULES_AFFILE_COLLECTION_COMPARATOR_H_

#include "syslog-ng.h"

typedef struct _CollectionComparator CollectionComparator;

typedef void (*cc_callback)(const gchar *value, gpointer user_data);

CollectionComparator *collection_comparator_new(void);
void collection_comparator_free(CollectionComparator *self);
void collection_comparator_start(CollectionComparator *self);
void collection_comparator_stop(CollectionComparator *self);
void collection_comparator_add_value(CollectionComparator *self, const gchar *value);
void collection_comparator_add_initial_value(CollectionComparator *self, const gchar *value);

void collection_comporator_set_callbacks(CollectionComparator *self, cc_callback handle_new, cc_callback handle_delete,
                                         gpointer user_data);


#endif /* MODULES_AFFILE_COLLECTION_COMPARATOR_H_ */
