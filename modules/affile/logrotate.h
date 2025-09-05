/*
 * Copyright (c) 2025 Alexander Zikulnig
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

#ifndef LOGROTATE_H_INCLUDED
#define LOGROTATE_H_INCLUDED

#include "syslog-ng.h"
#include <logproto/logproto.h>
#include "messages.h"

typedef enum
{
  LR_SUCCESS, LR_ERROR
} LogRotateStatus;

typedef struct _LogRotateOptions
{
  gboolean enable;
  gsize size;
  gsize max_rotations;
  gboolean pending;
} LogRotateOptions;

gboolean logrotate_is_enabled(LogRotateOptions *logrotate_options);
gboolean logrotate_is_required(LogRotateOptions *logrotate_options, const gsize filesize);
gboolean logrotate_is_pending(LogRotateOptions *logrotate_options);
void logrotate_options_defaults(LogRotateOptions *logrotate_options);
LogRotateStatus logrotate_do_rotate(LogRotateOptions *logrotate_options, const gchar *filename);

#endif
