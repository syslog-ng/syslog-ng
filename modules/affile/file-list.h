/*
 * Copyright (c) 2018 Balabit
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

#ifndef MODULES_AFFILE_FILE_LIST_H_
#define MODULES_AFFILE_FILE_LIST_H_

#include <glib.h>

typedef struct _FileList FileList;

FileList *file_list_new(void);
void file_list_free(FileList *self);

void file_list_add(FileList *self, const gchar *value);
gchar *file_list_pop(FileList *self);

gboolean file_list_remove(FileList *self, const gchar *value);

#endif /* MODULES_AFFILE_HASHED_QUEUE_H_ */
