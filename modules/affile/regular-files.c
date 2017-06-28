/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balázs Scheidler
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
#include "file-specializations.h"
#include "transport/transport-file.h"
#include "messages.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

static gboolean
_prepare_open(FileOpener *self, const gchar *name)
{
  struct stat st;

  if (stat(name, &st) >= 0)
    {
      if (S_ISFIFO(st.st_mode))
        {
          msg_error("You are using the file() driver, underlying file is a FIFO, it should be used by pipe()",
                    evt_tag_str("filename", name));
          errno = EINVAL;
          return FALSE;
        }
    }
  return TRUE;
}

static LogTransport *
_construct(FileOpener *self, gint fd)
{
  LogTransport *transport = log_transport_file_new(fd);

  transport->read = log_transport_file_read_and_ignore_eof_method;
  return transport;
}

FileOpener *
file_opener_for_regular_files_new(void)
{
  FileOpener *self = file_opener_new();

  self->prepare_open = _prepare_open;
  self->construct_transport = _construct;
  return self;
}
