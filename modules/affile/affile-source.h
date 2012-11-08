/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
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
  
#ifndef AFFILE_SOURCE_H_INCLUDED
#define AFFILE_SOURCE_H_INCLUDED

#include "driver.h"
#include "logreader.h"
#include "file-perms.h"

typedef struct _AFFileSourceDriver
{
  LogSrcDriver super;
  GString *filename;
  /* FIXME: the code assumes that reader is a LogReader at a lot of places, so this should be changed to LogReader */
  LogPipe *reader;
  LogReaderOptions reader_options;
  FilePermOptions file_perm_options;
  gint pad_size;
  guint32 flags;
  /* state information to follow a set of files using a wildcard expression */
} AFFileSourceDriver;

LogDriver *affile_sd_new(gchar *filename, guint32 flags);
void affile_sd_set_recursion(LogDriver *s, const gint recursion);
void affile_sd_set_pri_level(LogDriver *s, const gint16 severity);
void affile_sd_set_pri_facility(LogDriver *s, const gint16 facility);

#endif
