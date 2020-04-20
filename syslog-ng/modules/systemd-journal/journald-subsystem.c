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

#include "journald-subsystem.h"
#include <glib.h>
#include <gmodule.h>

#if SYSLOG_NG_SYSTEMD_JOURNAL_MODE == SYSLOG_NG_JOURNALD_SYSTEM

struct _Journald
{
  sd_journal *journal;
};

gboolean
load_journald_subsystem(void)
{
  return TRUE;
}

#else


#define LOAD_SYMBOL(library, symbol) g_module_symbol(library, #symbol, (gpointer*)&symbol)

#define JOURNAL_LIBRARY_NAME "libsystemd-journal.so.0"
#define SYSTEMD_LIBRARY_NAME "libsystemd.so.0"

const char *journald_libraries[] = {JOURNAL_LIBRARY_NAME, SYSTEMD_LIBRARY_NAME, NULL};

static GModule *journald_module;

typedef struct sd_journal sd_journal;

struct _Journald
{
  sd_journal *journal;
};

typedef int
(*SD_JOURNAL_OPEN)(sd_journal **ret, int flags);
typedef void
(*SD_JOURNAL_CLOSE)(sd_journal *j);
typedef int
(*SD_JOURNAL_SEEK_HEAD)(sd_journal *j);
typedef int
(*SD_JOURNAL_SEEK_TAIL)(sd_journal *j);
typedef int
(*SD_JOURNAL_GET_CURSOR)(sd_journal *j, char **cursor);
typedef int
(*SD_JOURNAL_NEXT)(sd_journal *j);
typedef void
(*SD_JOURNAL_RESTART_DATA)(sd_journal *j);
typedef int
(*SD_JOURNAL_ENUMERATE_DATA)(sd_journal *j, const void **data, size_t *length);
typedef int
(*SD_JOURNAL_SEEK_CURSOR)(sd_journal *j, const char *cursor);
typedef int
(*SD_JOURNAL_TEST_CURSOR)(sd_journal *j, const char *cursor);
typedef int
(*SD_JOURNAL_GET_FD)(sd_journal *j);
typedef int
(*SD_JOURNAL_PROCESS)(sd_journal *j);
typedef int
(*SD_JOURNAL_GET_REALTIME_USEC)(sd_journal *j, guint64 *usec);

static SD_JOURNAL_OPEN sd_journal_open;
static SD_JOURNAL_CLOSE sd_journal_close;
static SD_JOURNAL_SEEK_HEAD sd_journal_seek_head;
static SD_JOURNAL_SEEK_TAIL sd_journal_seek_tail;
static SD_JOURNAL_GET_CURSOR sd_journal_get_cursor;
static SD_JOURNAL_NEXT sd_journal_next;
static SD_JOURNAL_RESTART_DATA sd_journal_restart_data;
static SD_JOURNAL_ENUMERATE_DATA sd_journal_enumerate_data;
static SD_JOURNAL_SEEK_CURSOR sd_journal_seek_cursor;
static SD_JOURNAL_TEST_CURSOR sd_journal_test_cursor;
static SD_JOURNAL_GET_FD sd_journal_get_fd;
static SD_JOURNAL_PROCESS sd_journal_process;
static SD_JOURNAL_GET_REALTIME_USEC sd_journal_get_realtime_usec;

static GModule *
_journald_module_open(void)
{
  for (gint i = 0; (journald_libraries[i] != NULL) && !journald_module; i++)
    journald_module = g_module_open(journald_libraries[i], 0);

  return journald_module;
}

static gboolean
_load_journald_symbols(void)
{
  if (!LOAD_SYMBOL(journald_module, sd_journal_open))
    return FALSE;

  if (!LOAD_SYMBOL(journald_module, sd_journal_close))
    return FALSE;

  if (!LOAD_SYMBOL(journald_module, sd_journal_seek_head))
    return FALSE;

  if (!LOAD_SYMBOL(journald_module, sd_journal_seek_tail))
    return FALSE;

  if (!LOAD_SYMBOL(journald_module, sd_journal_get_cursor))
    return FALSE;

  if (!LOAD_SYMBOL(journald_module, sd_journal_next))
    return FALSE;

  if (!LOAD_SYMBOL(journald_module, sd_journal_restart_data))
    return FALSE;

  if (!LOAD_SYMBOL(journald_module, sd_journal_enumerate_data))
    return FALSE;

  if (!LOAD_SYMBOL(journald_module, sd_journal_seek_cursor))
    return FALSE;

  if (!LOAD_SYMBOL(journald_module, sd_journal_get_fd))
    return FALSE;

  if (!LOAD_SYMBOL(journald_module, sd_journal_process))
    return FALSE;

  if (!LOAD_SYMBOL(journald_module, sd_journal_get_realtime_usec))
    return FALSE;

  if (!LOAD_SYMBOL(journald_module, sd_journal_test_cursor))
    return FALSE;

  return TRUE;
}

gboolean
load_journald_subsystem(void)
{
  if (!journald_module)
    {
      journald_module = _journald_module_open();
      if (!journald_module)
        {
          return FALSE;
        }
      if (!_load_journald_symbols())
        {
          g_module_close(journald_module);
          journald_module = NULL;
          return FALSE;
        }
    }
  return TRUE;
}

#endif


int
journald_open(Journald *self, int flags)
{
  return sd_journal_open(&self->journal, flags);
}

void
journald_close(Journald *self)
{
  sd_journal_close(self->journal);
  self->journal = NULL;
}

int
journald_seek_head(Journald *self)
{
  return sd_journal_seek_head(self->journal);
}

int
journald_seek_tail(Journald *self)
{
  return sd_journal_seek_tail(self->journal);
}

int
journald_get_cursor(Journald *self, gchar **cursor)
{
  return sd_journal_get_cursor(self->journal, cursor);
}

int
journald_next(Journald *self)
{
  return sd_journal_next(self->journal);
}

void
journald_restart_data(Journald *self)
{
  sd_journal_restart_data(self->journal);
}

int
journald_enumerate_data(Journald *self, const void **data, gsize *length)
{
  return sd_journal_enumerate_data(self->journal, data, length);
}

int
journald_seek_cursor(Journald *self, const gchar *cursor)
{
  return sd_journal_seek_cursor(self->journal, cursor);
}

int
journald_test_cursor(Journald *self, const gchar *cursor)
{
  return sd_journal_test_cursor(self->journal, cursor);
}

int
journald_get_fd(Journald *self)
{
  return sd_journal_get_fd(self->journal);
}

int
journald_process(Journald *self)
{
  return sd_journal_process(self->journal);
}

int
journald_get_realtime_usec(Journald *self, guint64 *usec)
{
  return sd_journal_get_realtime_usec(self->journal, usec);
}

Journald *
journald_new(void)
{
  Journald *self = g_new0(Journald, 1);
  return self;
}

void
journald_free(Journald *self)
{
  g_free(self);
  return;
}
