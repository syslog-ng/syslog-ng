/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
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

#ifndef AFPROG_H_INCLUDED
#define AFPROG_H_INCLUDED

#include "driver.h"
#include "logwriter.h"

typedef struct _AFProgramDestDriver
{
  LogDriver super;
  GString *cmdline;
  LogPipe *writer;
  pid_t pid;
  LogWriterOptions writer_options;
} AFProgramDestDriver;


LogDriver *afprogram_dd_new(gchar *cmdline);

#endif
