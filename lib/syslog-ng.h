/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
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
  
#ifndef SYSLOG_NG_H_INCLUDED
#define SYSLOG_NG_H_INCLUDED

#include <config.h>

#if ENABLE_DMALLOC
#define USE_DMALLOC
#endif

#if ENABLE_DEBUG
#undef YYDEBUG
#define YYDEBUG 1
#endif

#define GLIB_DISABLE_DEPRECATION_WARNINGS 1

#include <glib.h>
#include "compat.h"
#include "versioning.h"

#define PATH_SYSLOG_NG_CONF     PATH_SYSCONFDIR "/syslog-ng.conf"
#define PATH_INSTALL_DAT	PATH_SYSCONFDIR "/install.dat"
#define PATH_PIDFILE            PATH_PIDFILEDIR "/syslog-ng.pid"
#define PATH_CONTROL_SOCKET     PATH_PIDFILEDIR "/syslog-ng.ctl"
#if ENABLE_ENV_WRAPPER
#define PATH_SYSLOGNG           PATH_LIBEXECDIR "/syslog-ng"
#endif
#define PATH_PERSIST_CONFIG     PATH_LOCALSTATEDIR "/syslog-ng.persist"

#define SAFE_STRING(x) ((x) ? (x) : "NULL")

typedef struct _LogPipe LogPipe;
typedef struct _LogMessage LogMessage;
typedef struct _GlobalConfig GlobalConfig;

/* configuration being parsed, used by the bison generated code, NULL whenever parsing is finished. */
extern GlobalConfig *configuration;
extern gchar *module_path;
extern gchar *default_modules;

#endif
