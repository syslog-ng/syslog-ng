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
#pragma once

#ifdef _WIN32

#include <stdarg.h>

/* Priorities */
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7
#define LOG_PRIMASK 0x07
#define LOG_PRI(p)  ((p) & LOG_PRIMASK)
#define LOG_MAKEPRI(fac, pri) ((fac) | (pri))
#define LOG_NFACILITIES 24 /* current number of facilities */

/* Facilities */
#define LOG_KERN    (0<<3)
#define LOG_USER    (1<<3)
#define LOG_MAIL    (2<<3)
#define LOG_DAEMON  (3<<3)
#define LOG_AUTH    (4<<3)
#define LOG_SYSLOG  (5<<3)
#define LOG_LPR     (6<<3)
#define LOG_NEWS    (7<<3)
#define LOG_UUCP    (8<<3)
#define LOG_CRON    (9<<3)
#define LOG_AUTHPRIV (10<<3)
#define LOG_FTP     (11<<3)
#define LOG_LOCAL0  (16<<3)
#define LOG_LOCAL1  (17<<3)
#define LOG_LOCAL2  (18<<3)
#define LOG_LOCAL3  (19<<3)
#define LOG_LOCAL4  (20<<3)
#define LOG_LOCAL5  (21<<3)
#define LOG_LOCAL6  (22<<3)
#define LOG_LOCAL7  (23<<3)
#define LOG_FACMASK 0x03f8
#define LOG_FAC(p)  (((p) & LOG_FACMASK) >> 3)

#ifdef SYSLOG_NAMES

#define INTERNAL_NOPRI 0x10        /* the "no priority" priority */
#define        INTERNAL_MARK        LOG_MAKEPRI(LOG_NFACILITIES, 0)
typedef struct _code
{
  char        *c_name;
  int        c_val;
} CODE;

extern const CODE prioritynames[];
extern const CODE facilitynames[];
#endif

#define LOG_MASK(pri) (1 << (pri))
#define LOG_UPTO(pri) ((1 << ((pri)+1)) - 1)

/*
 * Option flags for openlog.
 *
 * LOG_ODELAY no longer does anything.
 * LOG_NDELAY is the inverse of what it used to be.
 */
#define        LOG_PID                0x01        /* log the pid with each message */
#define        LOG_CONS        0x02        /* log on the console if errors in sending */
#define        LOG_ODELAY        0x04        /* delay open until first syslog() (default) */
#define        LOG_NDELAY        0x08        /* don't delay open */
#define        LOG_NOWAIT        0x10        /* don't wait for console forks: DEPRECATED */
#define        LOG_PERROR        0x20        /* log to stderr as well */

/* Optional prototypes (only used if someone actually calls them) */
void openlog(const char *ident, int option, int facility);
void syslog(int priority, const char *fmt, ...);
void closelog(void);

#else

#include <syslog.h>

#endif