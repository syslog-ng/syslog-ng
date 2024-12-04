/*
 * Copyright (c) 2024 Balabit
 * Copyright (c) 2024 Alexander Zikulnig
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
  DAILY, WEEKLY, MONTHLY, YEARLY, NONE
} interval_t;

typedef enum
{
  LR_SUCCESS, LR_ERROR
} LogRotateStatus;

typedef struct _LogRotateOptions
{
  gboolean enable;
  gsize size;
  gsize max_rotations;
  interval_t interval;
  const gchar *date_format;
} LogRotateOptions;

gboolean is_logrotate_enabled(LogRotateOptions *logrotate_options);
gboolean is_logrotate_pending(LogRotateOptions *logrotate_options, const gchar *filename);
void logrotate_options_defaults(LogRotateOptions *logrotate_options);
LogRotateStatus do_logrotate(LogRotateOptions *logrotate_options, const gchar *filename);

#endif
