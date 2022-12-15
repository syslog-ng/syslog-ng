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

typedef struct sd_journal sd_journal;

extern int (*sd_journal_open)(sd_journal **ret, int flags);
#if SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
extern int (*sd_journal_open_namespace)(sd_journal **ret, const char *namespace, int flags);
#endif
extern void (*sd_journal_close)(sd_journal *j);
extern int (*sd_journal_seek_head)(sd_journal *j);
extern int (*sd_journal_seek_tail)(sd_journal *j);
extern int (*sd_journal_get_cursor)(sd_journal *j, char **cursor);
extern int (*sd_journal_next)(sd_journal *j);
extern void (*sd_journal_restart_data)(sd_journal *j);
extern int (*sd_journal_enumerate_data)(sd_journal *j, const void **data, size_t *length);
extern int (*sd_journal_seek_cursor)(sd_journal *j, const char *cursor);
extern int (*sd_journal_test_cursor)(sd_journal *j, const char *cursor);
extern int (*sd_journal_get_fd)(sd_journal *j);
extern int (*sd_journal_process)(sd_journal *j);
extern int (*sd_journal_get_realtime_usec)(sd_journal *j, uint64_t *usec);
extern int (*sd_journal_add_match)(sd_journal *j, const void *data, size_t size);

int load_journald_subsystem(void);

#endif

#endif /* JOURNAL_SOURCE_INTERFACE_H_ */
