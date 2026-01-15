/*
 * Copyright (c) 2025 Airbus Commercial Aircraft
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

#ifndef _SLOG_FILE_H_INCLUDED_
#define _SLOG_FILE_H_INCLUDED_

#include <glib.h>

#include "slog_sect.h"

#define NUM_MODES 6
#define LEN_MODES 2

#define SLOG_FILE_DOMAIN 100
#define SLOG_FILE_READY 1000
#define SLOG_FILE_OPEN 1001
#define SLOG_FILE_CLOSED 1002
#define SLOG_FILE_GENERAL_ERROR 2000
#define SLOG_FILE_INVALID_MODE 2001
#define SLOG_FILE_EOF 2002
#define SLOG_FILE_RESOURCE_UNAVAILABLE 2003
#define SLOG_FILE_INCOMPLETE_READ 2004
#define SLOG_FILE_INCOMPLETE_WRITE 2005
#define SLOG_FILE_SHUTDOWN_ERROR 2006

// File abstraction
typedef struct
{
  const gchar *filename;
  const gchar *mode;
  GIOChannel *channel;
  GError *error;
  GIOStatus status;
  GString *message;
  unsigned int state;
} SLogFile;

// File operations
SLogFile *create_file(const gchar *filename, const gchar *mode);
gboolean open_file(SLogFile *f);
gboolean write_to_file(SLogFile *f, const gchar *data, gsize len);
gboolean read_from_file(SLogFile *f, gchar *data, gsize len);
gboolean read_line_from_file(SLogFile *f, GString *line);
gboolean close_file(SLogFile *f);

#endif
