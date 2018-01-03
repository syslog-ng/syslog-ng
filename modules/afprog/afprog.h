/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#ifndef AFPROG_H_INCLUDED
#define AFPROG_H_INCLUDED

#include "driver.h"
#include "logwriter.h"
#include "logreader.h"

typedef struct _AFProgramProcessInfo
{
  pid_t pid;
  GString *cmdline;
  gboolean inherit_environment;
} AFProgramProcessInfo;

typedef struct _AFProgramSourceDriver
{
  LogSrcDriver super;
  AFProgramProcessInfo process_info;
  LogReader *reader;
  LogReaderOptions reader_options;
} AFProgramSourceDriver;

typedef struct _AFProgramDestDriver
{
  LogDestDriver super;
  AFProgramProcessInfo process_info;
  LogWriter *writer;
  gboolean keep_alive;
  LogWriterOptions writer_options;
} AFProgramDestDriver;

LogDriver *afprogram_sd_new(gchar *cmdline, GlobalConfig *cfg);
LogDriver *afprogram_dd_new(gchar *cmdline, GlobalConfig *cfg);

void afprogram_dd_set_keep_alive(AFProgramDestDriver *self, gboolean keep_alive);
void afprogram_set_inherit_environment(AFProgramProcessInfo *self, gboolean inherit_environment);

#endif
