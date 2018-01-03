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

#ifndef AFFILE_FILE_OPENER_H_INCLUDED
#define AFFILE_FILE_OPENER_H_INCLUDED

/* portable largefile support for affile */
#include "compat/lfs.h"

#include "file-perms.h"
#include "transport/logtransport.h"
#include "logproto/logproto-server.h"
#include "logproto/logproto-client.h"
#include "logproto-file-reader.h"
#include <string.h>

typedef enum
{
  AFFILE_DIR_READ = 0x01,
  AFFILE_DIR_WRITE = 0x02,
} FileDirection;

typedef struct _FileOpenerOptions
{
  FilePermOptions file_perm_options;
  gboolean needs_privileges:1;
  gint create_dirs;
} FileOpenerOptions;

typedef struct _FileOpener FileOpener;
struct _FileOpener
{
  FileOpenerOptions *options;
  gboolean (*prepare_open)(FileOpener *self, const gchar *name);
  gint (*open)(FileOpener *self, const gchar *name, gint flags);
  gint (*get_open_flags)(FileOpener *self, FileDirection dir);
  LogTransport *(*construct_transport)(FileOpener *self, gint fd);
  LogProtoServer *(*construct_src_proto)(FileOpener *self, LogTransport *transport,
                                         LogProtoFileReaderOptions *proto_options);
  LogProtoClient *(*construct_dst_proto)(FileOpener *self, LogTransport *transport, LogProtoClientOptions *proto_options);
};

static inline LogTransport *
file_opener_construct_transport(FileOpener *self, gint fd)
{
  return self->construct_transport(self, fd);
}

static inline LogProtoServer *
file_opener_construct_src_proto(FileOpener *self, LogTransport *transport, LogProtoFileReaderOptions *proto_options)
{
  return self->construct_src_proto(self, transport, proto_options);
}

static inline LogProtoClient *
file_opener_construct_dst_proto(FileOpener *self, LogTransport *transport, LogProtoClientOptions *proto_options)
{
  return self->construct_dst_proto(self, transport, proto_options);
}

gboolean file_opener_open_fd(FileOpener *self, gchar *name, FileDirection dir, gint *fd);

void file_opener_set_options(FileOpener *self, FileOpenerOptions *options);
void file_opener_init_instance(FileOpener *self);
FileOpener *file_opener_new(void);
void file_opener_free(FileOpener *self);

void file_opener_options_defaults(FileOpenerOptions *options);
void file_opener_options_defaults_dont_change_permissions(FileOpenerOptions *options);
void file_opener_options_init(FileOpenerOptions *options, GlobalConfig *cfg);
void file_opener_options_deinit(FileOpenerOptions *options);


#endif
