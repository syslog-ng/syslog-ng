/*
 * Copyright (c) 2025 One Identity
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
#include "syslog.h"

#if defined(_WIN32)
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <process.h>

#if defined(_MSC_VER)
#define strdup _strdup
#endif

#ifdef SYSLOG_NAMES
extern const CODE prioritynames[] =
  {
    { "alert", LOG_ALERT },
    { "crit", LOG_CRIT },
    { "debug", LOG_DEBUG },
    { "emerg", LOG_EMERG },
    { "err", LOG_ERR },
    { "error", LOG_ERR },                /* DEPRECATED */
    { "info", LOG_INFO },
    { "none", INTERNAL_NOPRI },                /* INTERNAL */
    { "notice", LOG_NOTICE },
    { "panic", LOG_EMERG },                /* DEPRECATED */
    { "warn", LOG_WARNING },                /* DEPRECATED */
    { "warning", LOG_WARNING },
    { NULL, -1 }
  };

extern const CODE facilitynames[] =
  {
    { "auth", LOG_AUTH },
    { "authpriv", LOG_AUTHPRIV },
    { "cron", LOG_CRON },
    { "daemon", LOG_DAEMON },
    { "ftp", LOG_FTP },
    { "kern", LOG_KERN },
    { "lpr", LOG_LPR },
    { "mail", LOG_MAIL },
    { "mark", INTERNAL_MARK },                /* INTERNAL */
    { "news", LOG_NEWS },
    { "security", LOG_AUTH },                /* DEPRECATED */
    { "syslog", LOG_SYSLOG },
    { "user", LOG_USER },
    { "uucp", LOG_UUCP },
    { "local0", LOG_LOCAL0 },
    { "local1", LOG_LOCAL1 },
    { "local2", LOG_LOCAL2 },
    { "local3", LOG_LOCAL3 },
    { "local4", LOG_LOCAL4 },
    { "local5", LOG_LOCAL5 },
    { "local6", LOG_LOCAL6 },
    { "local7", LOG_LOCAL7 },
    { NULL, -1 }
  };
#endif

static char *g_ident   = NULL;
static int   g_options = 0;
static int   g_facility = LOG_USER; /* kept for API parity */

static void _syslog_vwrite(int priority, const char *fmt, va_list ap)
{
  (void)priority; (void)g_facility;

  char buf[1024];
  int n = vsnprintf(buf, sizeof(buf), fmt ? fmt : "", ap);
  if (n < 0) return;

  /* LOG_PERROR => always stderr here; LOG_CONS ignored on Windows */
  FILE *out = stderr;
  if (g_ident) {
    if (g_options & LOG_PID)
      fprintf(out, "%s[%d]: %s\n", g_ident, _getpid(), buf);
    else
      fprintf(out, "%s: %s\n", g_ident, buf);
  } else {
    fprintf(out, "%s\n", buf);
  }
  fflush(out);

  /* also send to debugger */
  OutputDebugStringA(buf);
  OutputDebugStringA("\n");
}

void openlog(const char *ident, int option, int facility)
{
  free(g_ident);
  g_ident   = ident ? strdup(ident) : NULL;
  g_options = option;
  g_facility = facility;
}

void syslog(int priority, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  _syslog_vwrite(priority, fmt, ap);
  va_end(ap);
}

void closelog(void)
{
  free(g_ident);
  g_ident = NULL;
  g_options = 0;
  g_facility = LOG_USER;
}
#endif /* _WIN32 */
