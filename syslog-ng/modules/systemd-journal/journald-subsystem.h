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

#include <stdlib.h>
#include <glib.h>
#include "syslog-ng-config.h"

#if SYSLOG_NG_SYSTEMD_JOURNAL_MODE == SYSLOG_NG_JOURNALD_SYSTEM
#include <systemd/sd-journal.h>
#else
/* Open flags */
enum
{
  SD_JOURNAL_LOCAL_ONLY = 1,
  SD_JOURNAL_RUNTIME_ONLY = 2,
  SD_JOURNAL_SYSTEM_ONLY = 4
};
#endif


typedef struct _Journald Journald;

typedef void (*FOREACH_DATA_CALLBACK)(gchar *key, gchar *value, gpointer user_data);

void journald_foreach_data(Journald *self, FOREACH_DATA_CALLBACK func, gpointer user_data);


gboolean load_journald_subsystem(void);
Journald *journald_new(void);
void journald_free(Journald *self);

int journald_open(Journald *self, int flags);
void journald_close(Journald *self);
int journald_seek_head(Journald *self);
int journald_seek_tail(Journald *self);
int journald_get_cursor(Journald *self, gchar **cursor);
int journald_next(Journald *self);
void journald_restart_data(Journald *self);
int journald_enumerate_data(Journald *self, const void **data, gsize *length);
int journald_seek_cursor(Journald *self, const gchar *cursor);
int journald_test_cursor(Journald *self, const gchar *cursor);
int journald_get_fd(Journald *self);
int journald_process(Journald *self);
int journald_get_realtime_usec(Journald *self, guint64 *usec);

#endif /* JOURNAL_SOURCE_INTERFACE_H_ */
