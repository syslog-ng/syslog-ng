/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
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
  
#ifndef SYSLOG_NG_H_INCLUDED
#define SYSLOG_NG_H_INCLUDED

#include <config.h>

#if ENABLE_DMALLOC
#define USE_DMALLOC
#endif

#if ENABLE_DEBUG
#define YYDEBUG 1
#endif

#include <glib.h>
#include "compat.h"

#if ENABLE_SQL || ENABLE_TIMESTAMPING
#define ENABLE_THREADS 1
#else
#define ENABLE_THREADS 0
#endif

#define PATH_SYSLOG_NG_CONF     PATH_SYSCONFDIR "/syslog-ng.conf"
#define PATH_PIDFILE            PATH_PIDFILEDIR "/syslog-ng.pid"
#define PATH_CONTROL_SOCKET     PATH_PIDFILEDIR "/syslog-ng.ctl"
#if ENABLE_ENV_WRAPPER
#define PATH_SYSLOGNG           PATH_PREFIX "/libexec/syslog-ng"
#endif

#define PATH_PERSIST_CONFIG     PATH_LOCALSTATEDIR "/syslog-ng.persist"
#define PATH_QDISK              PATH_LOCALSTATEDIR
#define PATH_PATTERNDB_FILE     PATH_LOCALSTATEDIR "/patterndb.xml"

#define LOG_PRIORITY_LISTEN 0
#define LOG_PRIORITY_READER 0
#define LOG_PRIORITY_WRITER -100
#define LOG_PRIORITY_CONNECT -150

#define SAFE_STRING(x) ((x) ? (x) : "NULL")

typedef struct _LogMessage LogMessage;
typedef struct _GlobalConfig GlobalConfig;

extern GlobalConfig *configuration;

void main_loop_wakeup(void);

#endif
