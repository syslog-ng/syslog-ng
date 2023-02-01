/*
 * Copyright (c) 2010-2014 Balabit
 * Copyright (c) 2010-2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#ifndef JOURNAL_SOURCE_INTERFACE_H_
#define JOURNAL_SOURCE_INTERFACE_H_

#include "syslog-ng-config.h"

#if SYSLOG_NG_SYSTEMD_JOURNAL_MODE == SYSLOG_NG_JOURNALD_SYSTEM
#include <systemd/sd-journal.h>

static inline int
load_journald_subsystem(void)
{
  return 1;
}

#else

#include <stddef.h>
#include <stdint.h>

/* Open flags */
enum
{
  SD_JOURNAL_LOCAL_ONLY = 1,
  SD_JOURNAL_RUNTIME_ONLY = 2,
  SD_JOURNAL_SYSTEM = 4,
  SD_JOURNAL_CURRENT_USER = 8,
  SD_JOURNAL_OS_ROOT = 16,
  SD_JOURNAL_ALL_NAMESPACES = 32,
  SD_JOURNAL_INCLUDE_DEFAULT_NAMESPACE = 64
};

typedef union sd_id128 sd_id128_t;

union sd_id128
{
  uint8_t bytes[16];
  uint64_t qwords[2];
};

#define SD_ID128_STRING_MAX 33


typedef struct sd_journal sd_journal;

/*
 * attribute((visibility("hidden")) is needed to avoid these variables to be
 * overridden with symbols of the same name from libsystemd.so.
 *
 * It may happen that libsyslog-ng.so itself is linked against libsystemd.so
 * (due to service management being used).  In this case these names would
 * map to actual functions in libsystemd.so, resulting in a nasty crash
 * whenever we try to use these as pointers to functions.
 *
 * By using "hidden" visibility, we tell the linker that these symbols are
 * internal to the current .so, meaning that any code that references these
 * from this module will happily use these as pointers while other modules
 * of syslog-ng can still use them as functions.
 *
 * See https://github.com/syslog-ng/syslog-ng/issues/4300 for more details.
 */

#define VISIBILITY_HIDDEN __attribute__((visibility("hidden")))

VISIBILITY_HIDDEN extern int (*sd_journal_open)(sd_journal **ret, int flags);
#if SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
VISIBILITY_HIDDEN extern int (*sd_journal_open_namespace)(sd_journal **ret, const char *namespace, int flags);
#endif
VISIBILITY_HIDDEN extern void (*sd_journal_close)(sd_journal *j);
VISIBILITY_HIDDEN extern int (*sd_journal_seek_head)(sd_journal *j);
VISIBILITY_HIDDEN extern int (*sd_journal_seek_tail)(sd_journal *j);
VISIBILITY_HIDDEN extern int (*sd_journal_get_cursor)(sd_journal *j, char **cursor);
VISIBILITY_HIDDEN extern int (*sd_journal_next)(sd_journal *j);
VISIBILITY_HIDDEN extern void (*sd_journal_restart_data)(sd_journal *j);
VISIBILITY_HIDDEN extern int (*sd_journal_enumerate_data)(sd_journal *j, const void **data, size_t *length);
VISIBILITY_HIDDEN extern int (*sd_journal_seek_cursor)(sd_journal *j, const char *cursor);
VISIBILITY_HIDDEN extern int (*sd_journal_test_cursor)(sd_journal *j, const char *cursor);
VISIBILITY_HIDDEN extern int (*sd_journal_get_fd)(sd_journal *j);
VISIBILITY_HIDDEN extern int (*sd_journal_process)(sd_journal *j);
VISIBILITY_HIDDEN extern int (*sd_journal_get_realtime_usec)(sd_journal *j, uint64_t *usec);
VISIBILITY_HIDDEN extern int (*sd_journal_add_match)(sd_journal *j, const void *data, size_t size);
VISIBILITY_HIDDEN extern char *(*sd_id128_to_string)(sd_id128_t id, char s[SD_ID128_STRING_MAX]);
VISIBILITY_HIDDEN extern int (*sd_id128_get_boot)(sd_id128_t *ret);

int load_journald_subsystem(void);

#endif

#endif /* JOURNAL_SOURCE_INTERFACE_H_ */
