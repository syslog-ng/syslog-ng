/*
 * Copyright (c) 2014      BalaBit S.a.r.l., Luxembourg, Luxembourg
 * Copyright (c) 2010-2014 BalaBit IT Ltd, Budapest, Hungary
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

#define LOAD_SYMBOL(library, symbol) g_module_symbol(library, #symbol, (gpointer*)&symbol)

#define JOURNAL_LIBRARY_NAME "libsystemd-journal.so.0"

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
(*SD_JOURNAL_GET_FD)(sd_journal *j);
typedef int
(*SD_JOURNAL_PROCESS)(sd_journal *j);
typedef int
(*SD_JOURNAL_GET_REALTIME_USEC)(sd_journal *j, guint64 *usec);

SD_JOURNAL_OPEN sd_journal_open;
SD_JOURNAL_CLOSE sd_journal_close;
SD_JOURNAL_SEEK_HEAD sd_journal_seek_head;
SD_JOURNAL_SEEK_TAIL sd_journal_seek_tail;
SD_JOURNAL_GET_CURSOR sd_journal_get_cursor;
SD_JOURNAL_NEXT sd_journal_next;
SD_JOURNAL_RESTART_DATA sd_journal_restart_data;
SD_JOURNAL_ENUMERATE_DATA sd_journal_enumerate_data;
SD_JOURNAL_SEEK_CURSOR sd_journal_seek_cursor;
SD_JOURNAL_GET_FD sd_journal_get_fd;
SD_JOURNAL_PROCESS sd_journal_process;
SD_JOURNAL_GET_REALTIME_USEC sd_journal_get_realtime_usec;

gboolean
load_journald_subsystem()
{
  if (!journald_module)
    {
      journald_module = g_module_open(JOURNAL_LIBRARY_NAME, 0);
      if (!journald_module)
        {
          return FALSE;
        }
      if (!LOAD_SYMBOL(journald_module, sd_journal_open))
        {
          goto error;
        }
      if (!LOAD_SYMBOL(journald_module, sd_journal_close))
        {
          goto error;
        }
      if (!LOAD_SYMBOL(journald_module, sd_journal_seek_head))
        {
          goto error;
        }
      if (!LOAD_SYMBOL(journald_module, sd_journal_seek_tail))
        {
          goto error;
        }
      if (!LOAD_SYMBOL(journald_module, sd_journal_get_cursor))
        {
          goto error;
        }
      if (!LOAD_SYMBOL(journald_module, sd_journal_next))
        {
          goto error;
        }
      if (!LOAD_SYMBOL(journald_module, sd_journal_restart_data))
        {
          goto error;
        }
      if (!LOAD_SYMBOL(journald_module, sd_journal_enumerate_data))
        {
          goto error;
        }
      if (!LOAD_SYMBOL(journald_module, sd_journal_seek_cursor))
        {
          goto error;
        }
      if (!LOAD_SYMBOL(journald_module, sd_journal_get_fd))
        {
          goto error;
        }
      if (!LOAD_SYMBOL(journald_module, sd_journal_process))
        {
          goto error;
        }
      if (!LOAD_SYMBOL(journald_module, sd_journal_get_realtime_usec))
        {
          goto error;
        }
    }
  return TRUE;
  error: g_module_close(journald_module);
  journald_module = NULL;
  return FALSE;
}

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
journald_new()
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
