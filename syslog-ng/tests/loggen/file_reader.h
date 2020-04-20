/*
 * Copyright (c) 2018 Balabit
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

#ifndef LOGGEN_FILE_READER_H_INCLUDED
#define LOGGEN_FILE_READER_H_INCLUDED

#include "compat/glib.h"
#include "compat/compat.h"

/* RFC5424 specific constants */
#define RFC5424_NIL_VALUE "-"
#define RFC5424_DELIMITERS " >"
#define RFC5424_BOM "﻿"

#define RFC5424_HEADER_TOKEN_NUM 8
#define RFC5424_PRI_INDEX 0
#define RFC5424_VER_INDEX 1
#define RFC5424_TIMESTAMP_INDEX 2
#define RFC5424_HOST_NAME_INDEX 3
#define RFC5424_APP_NAME_INDEX 4
#define RFC5424_PID_INDEX 5
#define RFC5424_MSGID_INDEX 6
#define RFC5424_SDATA_INDEX 7

/* RFC3164 specific constants */
#define RFC3164_TIMESTAMP_SIZE 15

/* buffer & constants for parsed messages */
#define PARS_BUF_PRI_SIZE 3+1
#define PARS_BUF_VER_SIZE 2+1
#define PARS_BUF_TIME_STAMP_SIZE 128+1
#define PARS_BUF_HOST_SIZE 255+1
#define PARS_BUF_APP_SIZE 48+1
#define PARS_BUF_MSG_ID_SIZE 32+1
#define PARS_BUF_PID_SIZE 128+1
#define PARS_BUF_SDATA_SIZE 1024+1
#define PARS_BUF_MSG_SIZE 4096+1

typedef enum _LogFormatType
{
  LOG_FORMAT_UNKNOWN,
  LOG_FORMAT_RFC5424,
  LOG_FORMAT_RFC3164
} LogFormatType;

typedef struct _SyslogMsgElements
{
  gchar pri[PARS_BUF_PRI_SIZE];
  gchar ver[PARS_BUF_VER_SIZE];
  gchar time_stamp[PARS_BUF_TIME_STAMP_SIZE];
  gchar host[PARS_BUF_HOST_SIZE];
  gchar app[PARS_BUF_APP_SIZE];
  gchar pid[PARS_BUF_PID_SIZE];
  gchar msgid[PARS_BUF_MSG_ID_SIZE];
  gchar sdata[PARS_BUF_SDATA_SIZE];
  gchar message[PARS_BUF_MSG_SIZE];
} SyslogMsgElements;

GOptionEntry *get_file_reader_options(void);
int read_next_message_from_file(char *buf, int buflen, int syslog_proto, int thread_index);
int init_file_reader(int nr_threads);
void close_file_reader(int nr_threads);

#endif
