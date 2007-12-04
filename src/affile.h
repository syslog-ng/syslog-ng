/*
 * Copyright (c) 2002-2007 BalaBit IT Ltd, Budapest, Hungary                    
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

typedef struct _AFFileSourceDriver
{
  LogDriver super;
  GString *filename;
  LogPipe *reader;
  LogReaderOptions reader_options;
  guint32 flags;
} AFFileSourceDriver;

LogDriver *affile_sd_new(gchar *filename, guint32 flags);

typedef struct _AFFileDestDriver
{
  LogDriver super;
  LogTemplate *filename_template;
  LogPipe *writer;
  guint32 flags;
  uid_t file_uid;
  gid_t file_gid;
  mode_t file_perm;
  uid_t dir_uid;
  gid_t dir_gid;
  mode_t dir_perm;
  LogWriterOptions writer_options;
  GHashTable *writer_hash;
  GlobalConfig *cfg;
  
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

#endif
