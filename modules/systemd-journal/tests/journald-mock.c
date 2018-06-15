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

#include "journald-mock.h"
#include "fdhelpers.h"

#include <fcntl.h>
#include <unistd.h>
#include <glib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>


struct _Journald
{
  int fds[2];
  GList *entries;
  GList *current_pos;
  GList *next_element;
  gboolean opened;
};

struct _MockEntry
{
  GPtrArray *data;
  gint index;
  gchar *cursor;
};

int
journald_open(Journald *self, int flags)
{
  self->opened = TRUE;
  return 0;
}

void
journald_close(Journald *self)
{
  self->opened = FALSE;
}

int
journald_seek_head(Journald *self)
{
  g_assert(self->opened);
  self->next_element = g_list_first(self->entries);
  return 0;
}

int
journald_seek_tail(Journald *self)
{
  g_assert(self->opened);
  self->next_element = g_list_last(self->entries);
  return 0;
}

int
journald_get_cursor(Journald *self, gchar **cursor)
{
  g_assert(self->opened);
  MockEntry *entry = (MockEntry *)self->current_pos->data;
  *cursor = g_strdup(entry->cursor);
  return 0;
}

int
journald_next(Journald *self)
{
  g_assert(self->opened);
  self->current_pos = self->next_element;
  if (self->current_pos)
    {
      self->next_element = self->current_pos->next;
      return 1;
    }
  return 0;
}

void
journald_restart_data(Journald *self)
{
  g_assert(self->opened);
  MockEntry *entry = (MockEntry *)self->current_pos->data;
  entry->index = 0;
}

int
journald_enumerate_data(Journald *self, const void **data, gsize *length)
{
  g_assert(self->opened);
  MockEntry *entry = (MockEntry *)self->current_pos->data;
  if (entry->index >= entry->data->len)
    {
      return 0;
    }
  *data = entry->data->pdata[entry->index];
  *length = strlen(*data);
  entry->index++;
  return 1;
}

static gint
_compare_mock_entries(gconstpointer a, gconstpointer b)
{
  const MockEntry *entry = a;
  const gchar *cursor = b;
  return strcmp(entry->cursor, cursor);
}

int
journald_seek_cursor(Journald *self, const gchar *cursor)
{
  g_assert(self->opened);
  GList *found_element = g_list_find_custom(self->entries, cursor, _compare_mock_entries);
  if (found_element)
    {
      self->next_element = found_element;
      return 0;
    }
  else
    {
      errno = ENOENT;
      return -1;
    }
}

int
journald_test_cursor(Journald *self, const gchar *cursor)
{
  return 1;
}

int
journald_get_fd(Journald *self)
{
  g_assert(self->opened);
  return self->fds[0];
}

int
journald_process(Journald *self)
{
  g_assert(self->opened);
  guint8 data;
  gint res = 1;
  while(res > 0)
    {
      res = read(self->fds[0], &data, 1);
      if (res < 0 && errno != EAGAIN)
        {
          fprintf(stderr, "JournaldMOCK: Can't read the pipe's fd: %s\n", strerror(errno));
        }
    }
  return 0;
}

int
journald_get_realtime_usec(Journald *self, guint64 *usec)
{
  *usec = 1408967385496986;
  return 0;
}

Journald *
journald_mock_new(void)
{
  Journald *self = g_new0(Journald, 1);

  int result = pipe(self->fds);
  g_assert(result == 0);
  g_fd_set_nonblock(self->fds[0], TRUE);
  g_fd_set_nonblock(self->fds[1], TRUE);

  return self;
}

MockEntry *
mock_entry_new(const gchar *cursor)
{
  MockEntry *self = g_new0(MockEntry, 1);
  self->cursor = g_strdup(cursor);
  self->data = g_ptr_array_new();
  return self;
}

void
mock_entry_add_data(MockEntry *self, gchar *data)
{
  g_ptr_array_add(self->data, data);
}

void
mock_entry_free(gpointer s)
{
  MockEntry *self = (MockEntry *)s;
  g_free(self->cursor);
  g_ptr_array_free(self->data, TRUE);
  g_free(self);
}

void
journald_mock_add_entry(Journald *self, MockEntry *entry)
{
  gboolean first_element = (self->entries == NULL);
  self->entries = g_list_append(self->entries, entry);
  gint res = write(self->fds[1], "1", 1);
  if (res < 0)
    {
      fprintf(stderr, "JournaldMOCK: Can't write the pipe's fd: %s\n", strerror(errno));
    }
  if (first_element && !self->next_element)
    {
      self->next_element = self->entries;
    }
}

void
journald_free(Journald *self)
{
  close(self->fds[0]);
  close(self->fds[1]);
  g_list_free_full(self->entries, mock_entry_free);
  g_free(self);
  return;
}
