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

#ifndef TAGGER_SCANNER_H_INCLUDED
#define TAGGER_SCANNER_H_INCLUDED

#include <syslog-ng.h>
#include <stdio.h>
#include "kvtagdb.h"

typedef struct _TagRecordScanner TagRecordScanner;

struct _TagRecordScanner
{
  TagRecord last_record;
  gpointer scanner;
  const TagRecord* (*get_next)(TagRecordScanner *self, const gchar *input);
  void (*free_fn)(TagRecordScanner *self);
};

void tag_record_scanner_free(TagRecordScanner *self);

#endif
