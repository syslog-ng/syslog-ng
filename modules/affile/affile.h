/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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
  
#ifndef SDFILE_H_INCLUDED
#define SDFILE_H_INCLUDED

#include "driver.h"
#include "logreader.h"
#include "logwriter.h"

#define AFFILE_PIPE        0x00000001
#define AFFILE_NO_EXPAND   0x00000002
#define AFFILE_TMPL_ESCAPE 0x00000004
#define AFFILE_CREATE_DIRS 0x00000008
#define AFFILE_FSYNC       0x00000010
#define AFFILE_PRIVILEGED  0x00000020

typedef struct _AFFileSourceDriver
{
  LogDriver super;
  GString *filename;
  /* FIXME: the code assumes that reader is a LogReader at a lot of places, so this should be changed to LogReader */
  LogPipe *reader;
  LogReaderOptions reader_options;
  guint32 flags;
  /* state information to follow a set of files using a wildcard expression */
} AFFileSourceDriver;

LogDriver *affile_sd_new(gchar *filename, guint32 flags);
void affile_sd_set_recursion(LogDriver *s, const gint recursion);
void affile_sd_set_pri_level(LogDriver *s, const gint16 severity);
void affile_sd_set_pri_facility(LogDriver *s, const gint16 facility);

typedef struct _AFFileDestWriter AFFileDestWriter;

typedef struct _AFFileDestDriver
{
  LogDriver super;
  LogTemplate *filename_template;
  AFFileDestWriter *single_writer;
  guint32 flags;
  gint file_uid;
  gint file_gid;
  gint file_perm;
  gint dir_uid;
  gint dir_gid;
  gint dir_perm;
  gchar *local_time_zone;
  TimeZoneInfo *local_time_zone_info;
  LogWriterOptions writer_options;
  GHashTable *writer_hash;
    
  gint overwrite_if_older;
  gboolean use_time_recvd;
  gint time_reap;
  guint reap_timer;
} AFFileDestDriver;

LogDriver *affile_dd_new(gchar *filename, guint32 flags);

void affile_dd_set_compress(LogDriver *s, gboolean compress);
void affile_dd_set_encrypt(LogDriver *s, gboolean enable);
void affile_dd_set_file_uid(LogDriver *s, const gchar *file_uid);
void affile_dd_set_file_gid(LogDriver *s, const gchar *file_gid);
void affile_dd_set_file_perm(LogDriver *s, mode_t file_perm);
void affile_dd_set_dir_uid(LogDriver *s, const gchar *dir_uid);
void affile_dd_set_dir_gid(LogDriver *s, const gchar *dir_gid);
void affile_dd_set_dir_perm(LogDriver *s, mode_t dir_perm);
void affile_dd_set_create_dirs(LogDriver *s, gboolean create_dirs);
void affile_dd_set_fsync(LogDriver *s, gboolean enable);
void affile_dd_set_overwrite_if_older(LogDriver *s, gint overwrite_if_older);
void affile_dd_set_local_time_zone(LogDriver *s, const gchar *local_time_zone);

#endif
