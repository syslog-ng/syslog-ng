/*
 * Copyright (c) 2016 Balabit
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
#ifndef LIST_SCANNER_H_INCLUDED
#define LIST_SCANNER_H_INCLUDED

#include "syslog-ng.h"

typedef struct _ListScanner ListScanner;
struct _ListScanner
{
  gint argc;
  gchar **argv;
  GPtrArray *argv_buffer;
  GString *value;
  gboolean free_argv_payload;
  const gchar *current_arg;
  gint current_arg_ndx;
  gboolean current_arg_split;
};

void list_scanner_input_va(ListScanner *self, const gchar *arg1, ...);
void list_scanner_input_gstring_array(ListScanner *self, gint argc, GString *argv[]);
gboolean list_scanner_scan_next(ListScanner *self);
const gchar *list_scanner_get_current_value(ListScanner *self);
gsize list_scanner_get_current_value_len(ListScanner *self);
ListScanner *list_scanner_clone(ListScanner *self);
void list_scanner_free_method(ListScanner *self);
void list_scanner_init(ListScanner *self);
void list_scanner_deinit(ListScanner *self);

ListScanner *list_scanner_new(void);
void list_scanner_free(ListScanner *self);

#endif
