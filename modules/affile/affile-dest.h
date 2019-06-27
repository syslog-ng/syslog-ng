/*
 * Copyright (c) 2002-2013 Balabit
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

#ifndef AFFILE_DEST_H_INCLUDED
#define AFFILE_DEST_H_INCLUDED

#include "driver.h"
#include "logwriter.h"
#include "file-opener.h"

typedef struct _AFFileDestWriter AFFileDestWriter;

typedef struct _AFFileDestDriver
{
  LogDestDriver super;
  GStaticMutex lock;
  LogTemplate *filename_template;
  AFFileDestWriter *single_writer;
  gboolean filename_is_a_template;
  gboolean template_escape;
  gboolean use_fsync;
  FileOpenerOptions file_opener_options;
  FileOpener *file_opener;
  TimeZoneInfo *local_time_zone_info;
  LogWriterOptions writer_options;
  guint32 writer_flags;
  GHashTable *writer_hash;

  gint overwrite_if_older;
  gboolean use_time_recvd;
  gint time_reap;
} AFFileDestDriver;

AFFileDestDriver *affile_dd_new_instance(gchar *filename, GlobalConfig *cfg);
LogDriver *affile_dd_new(gchar *filename, GlobalConfig *cfg);

void affile_dd_set_create_dirs(LogDriver *s, gboolean create_dirs);
void affile_dd_set_fsync(LogDriver *s, gboolean enable);
void affile_dd_set_overwrite_if_older(LogDriver *s, gint overwrite_if_older);
void affile_dd_set_local_time_zone(LogDriver *s, const gchar *local_time_zone);
void affile_dd_set_time_reap(LogDriver *s, gint time_reap);
void affile_dd_global_init(void);

#endif
