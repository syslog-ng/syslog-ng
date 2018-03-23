/*
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Balazs Scheidler <bazsi@balabit.hu>
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

#ifndef FILE_PERMS_H_INCLUDED
#define FILE_PERMS_H_INCLUDED

#include "syslog-ng.h"

typedef struct _FilePermOptions
{
  gint file_uid;
  gint file_gid;
  gint file_perm;
  gint dir_uid;
  gint dir_gid;
  gint dir_perm;
} FilePermOptions;

void file_perm_options_set_file_uid(FilePermOptions *s, const gchar *file_uid);
void file_perm_options_dont_change_file_uid(FilePermOptions *s);
void file_perm_options_set_file_gid(FilePermOptions *s, const gchar *file_gid);
void file_perm_options_dont_change_file_gid(FilePermOptions *s);
void file_perm_options_set_file_perm(FilePermOptions *s, gint file_perm);
void file_perm_options_dont_change_file_perm(FilePermOptions *s);
void file_perm_options_set_dir_uid(FilePermOptions *s, const gchar *dir_uid);
void file_perm_options_dont_change_dir_uid(FilePermOptions *s);
void file_perm_options_set_dir_gid(FilePermOptions *s, const gchar *dir_gid);
void file_perm_options_dont_change_dir_gid(FilePermOptions *s);
void file_perm_options_set_dir_perm(FilePermOptions *s, gint dir_perm);
void file_perm_options_dont_change_dir_perm(FilePermOptions *s);

void file_perm_options_defaults(FilePermOptions *self);
void file_perm_options_global_defaults(FilePermOptions *self);
void file_perm_options_inherit_from(FilePermOptions *self, const FilePermOptions *from);
void file_perm_options_inherit_dont_change(FilePermOptions *self);

gboolean file_perm_options_apply_file(const FilePermOptions *self, const gchar *name);
gboolean file_perm_options_apply_dir(const FilePermOptions *self, const gchar *name);
gboolean file_perm_options_apply_fd(const FilePermOptions *self, gint fd);
gboolean file_perm_options_create_containing_directory(const FilePermOptions *self, const gchar *name);

#endif
