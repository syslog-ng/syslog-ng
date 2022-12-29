/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 1998-2016 Balázs Scheidler
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

#ifndef DBPARSER_PDB_FILE_H_INCLUDED
#define DBPARSER_PDB_FILE_H_INCLUDED 1

#include "syslog-ng.h"

gint pdb_file_detect_version(const gchar *pdbfile, GError **error);
gboolean pdb_file_validate(const gchar *filename, GError **error);
gboolean pdb_file_validate_in_tests(const gchar *filename, GError **error);
GPtrArray *pdb_get_filenames(const gchar *dir_path, gboolean recursive, gchar *pattern, GError **error);
void pdb_sort_filenames(GPtrArray *filenames);

#endif
