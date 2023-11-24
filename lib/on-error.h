/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#ifndef ON_ERROR_H_INCLUDED
#define ON_ERROR_H_INCLUDED

#include "syslog-ng.h"
#include "atomic.h"

typedef enum
{
  ON_ERROR_DROP_MESSAGE        = 0x01,
  ON_ERROR_DROP_PROPERTY       = 0x02,
  ON_ERROR_FALLBACK_TO_STRING  = 0x04, /* Valid for type hinting
                                          only! */
  ON_ERROR_SILENT              = 0x08
} OnError;

gboolean on_error_parse(const gchar *on_error, gint *out);

#endif
