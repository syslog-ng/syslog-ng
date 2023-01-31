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

#if SYSLOG_NG_SYSTEMD_JOURNAL_MODE == SYSLOG_NG_JOURNALD_SYSTEM
# define MOCK_FUNC(f) f
# define MOCK_STATIC
#else
# define MOCK_FUNC(f) _##f
# define MOCK_STATIC static
#endif /* #if SYSLOG_NG_SYSTEMD_JOURNAL_MODE == SYSLOG_NG_JOURNALD_SYSTEM */

typedef struct _sd_journal_mock sd_journal_mock;
typedef struct _MockJournal MockJournal;

static void sd_journal_mock_journal_updated(sd_journal_mock *self);

struct _MockEntry
{
  GPtrArray *data;
  gint index;
  gchar *cursor;
};

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

struct _MockJournal
{
  GList *entries;
};

static MockJournal journal_contents;
static sd_journal_mock *sd_journal_under_test = NULL;

void
mock_journal_add_entry(MockEntry *entry)
{
  journal_contents.entries = g_list_append(journal_contents.entries, entry);

  /* if sd_journal_under_test is NULL, it means we are just collecting journal elements */
  if (sd_journal_under_test)
    sd_journal_mock_journal_updated(sd_journal_under_test);
}

void
mock_journal_free(void)
{
  g_list_free_full(journal_contents.entries, mock_entry_free);
  journal_contents.entries = NULL;
}

struct _sd_journal_mock
{
  int fds[2];
  MockJournal *journal_contents;
  GList *current_pos;
  GList *next_element;
  gboolean opened;
};

static sd_journal_mock *
sd_journal_mock_new(void)
{
  sd_journal_mock *self = g_new0(sd_journal_mock, 1);

  int result = pipe(self->fds);
  g_assert(result == 0);
  g_fd_set_nonblock(self->fds[0], TRUE);
  g_fd_set_nonblock(self->fds[1], TRUE);

  g_assert(sd_journal_under_test == NULL);
  sd_journal_under_test = self;

  self->journal_contents = &journal_contents;
  return self;
}

static void
sd_journal_mock_journal_updated(sd_journal_mock *self)
{
  gboolean first_element = g_list_length(self->journal_contents->entries) == 1;
  gint res = write(self->fds[1], "1", 1);
  if (res < 0)
    {
      fprintf(stderr, "JournaldMOCK: Can't write the pipe's fd: %s\n", strerror(errno));
    }
  if (first_element && !self->next_element)
    {
      self->next_element = self->journal_contents->entries;
    }
}


MOCK_STATIC int
MOCK_FUNC(sd_journal_open)(sd_journal **s, int flags)
{
  sd_journal_mock *self = (sd_journal_mock *) sd_journal_mock_new();
  self->opened = TRUE;
  *s = (sd_journal *) self;
  return 0;
}

MOCK_STATIC int
MOCK_FUNC(sd_journal_open_namespace)(sd_journal **s, const gchar *namespace, int flags)
{
  return sd_journal_open(s, flags);
}

MOCK_STATIC void
MOCK_FUNC(sd_journal_close)(sd_journal *s)
{
  sd_journal_mock *self = (sd_journal_mock *) s;
  self->opened = FALSE;
  close(self->fds[0]);
  close(self->fds[1]);
  g_free(self);

  g_assert(sd_journal_under_test == self);
  sd_journal_under_test = NULL;
}

MOCK_STATIC int
MOCK_FUNC(sd_journal_seek_head)(sd_journal *s)
{
  sd_journal_mock *self = (sd_journal_mock *) s;

  g_assert(self->opened);
  self->next_element = g_list_first(self->journal_contents->entries);
  return 0;
}

MOCK_STATIC int
MOCK_FUNC(sd_journal_seek_tail)(sd_journal *s)
{
  sd_journal_mock *self = (sd_journal_mock *) s;

  g_assert(self->opened);
  self->next_element = g_list_last(self->journal_contents->entries);
  return 0;
}

MOCK_STATIC int
MOCK_FUNC(sd_journal_get_cursor)(sd_journal *s, gchar **cursor)
{
  sd_journal_mock *self = (sd_journal_mock *) s;

  g_assert(self->opened);
  MockEntry *entry = (MockEntry *)self->current_pos->data;
  *cursor = g_strdup(entry->cursor);
  return 0;
}

MOCK_STATIC int
MOCK_FUNC(sd_journal_next)(sd_journal *s)
{
  sd_journal_mock *self = (sd_journal_mock *) s;

  g_assert(self->opened);
  self->current_pos = self->next_element;
  if (self->current_pos)
    {
      self->next_element = self->current_pos->next;
      return 1;
    }
  return 0;
}

MOCK_STATIC void
MOCK_FUNC(sd_journal_restart_data)(sd_journal *s)
{
  sd_journal_mock *self = (sd_journal_mock *) s;

  g_assert(self->opened);
  MockEntry *entry = (MockEntry *)self->current_pos->data;
  entry->index = 0;
}

MOCK_STATIC int
MOCK_FUNC(sd_journal_enumerate_data)(sd_journal *s, const void **data, gsize *length)
{
  sd_journal_mock *self = (sd_journal_mock *) s;

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

MOCK_STATIC int
MOCK_FUNC(sd_journal_seek_cursor)(sd_journal *s, const gchar *cursor)
{
  sd_journal_mock *self = (sd_journal_mock *) s;

  g_assert(self->opened);
  GList *found_element = g_list_find_custom(self->journal_contents->entries, cursor, _compare_mock_entries);
  if (found_element)
    {
      self->next_element = found_element;
      return 0;
    }
  else
    {
      errno = ENOENT;
      return -ENOENT;
    }
}

MOCK_STATIC int
MOCK_FUNC(sd_journal_test_cursor)(sd_journal *self, const gchar *cursor)
{
  return 1;
}

MOCK_STATIC int
MOCK_FUNC(sd_journal_get_fd)(sd_journal *s)
{
  sd_journal_mock *self = (sd_journal_mock *) s;
  g_assert(self->opened);
  return self->fds[0];
}

MOCK_STATIC int
MOCK_FUNC(sd_journal_process)(sd_journal *s)
{
  sd_journal_mock *self = (sd_journal_mock *) s;

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

MOCK_STATIC int
MOCK_FUNC(sd_journal_get_realtime_usec)(sd_journal *s, guint64 *usec)
{
  *usec = 1408967385496986;
  return 0;
}

MOCK_STATIC int
MOCK_FUNC(sd_journal_add_match)(sd_journal *s, const void *match, size_t match_len)
{
  return 0;
}

MOCK_STATIC char *
MOCK_FUNC(sd_id128_to_string)(sd_id128_t id, char s[SD_ID128_STRING_MAX])
{
  return s;
}

MOCK_STATIC int
MOCK_FUNC(sd_id128_get_boot)(sd_id128_t *ret)
{
  return 0;
}

#if SYSLOG_NG_SYSTEMD_JOURNAL_MODE != SYSLOG_NG_JOURNALD_SYSTEM

int (*sd_journal_open)(sd_journal **ret, int flags) = _sd_journal_open;
int (*sd_journal_open_namespace)(sd_journal **ret, const char *namespace, int flags) = _sd_journal_open_namespace;
void (*sd_journal_close)(sd_journal *j) = _sd_journal_close;
int (*sd_journal_seek_head)(sd_journal *j) = _sd_journal_seek_head;
int (*sd_journal_seek_tail)(sd_journal *j) = _sd_journal_seek_tail;
int (*sd_journal_get_cursor)(sd_journal *j, char **cursor) = _sd_journal_get_cursor;
int (*sd_journal_next)(sd_journal *j) = _sd_journal_next;
void (*sd_journal_restart_data)(sd_journal *j) = _sd_journal_restart_data;
int (*sd_journal_enumerate_data)(sd_journal *j, const void **data, size_t *length) = _sd_journal_enumerate_data;
int (*sd_journal_seek_cursor)(sd_journal *j, const char *cursor) = _sd_journal_seek_cursor;
int (*sd_journal_test_cursor)(sd_journal *j, const char *cursor) = _sd_journal_test_cursor;
int (*sd_journal_get_fd)(sd_journal *j) = _sd_journal_get_fd;
int (*sd_journal_process)(sd_journal *j) = _sd_journal_process;
int (*sd_journal_get_realtime_usec)(sd_journal *j, uint64_t *usec) = _sd_journal_get_realtime_usec;
int (*sd_journal_add_match)(sd_journal *j, const void *data, size_t size) = _sd_journal_add_match;
char *(*sd_id128_to_string)(sd_id128_t id, char s[SD_ID128_STRING_MAX]) = _sd_id128_to_string;
int (*sd_id128_get_boot)(sd_id128_t *ret) = _sd_id128_get_boot;

#endif // #if SYSLOG_NG_SYSTEMD_JOURNAL_MODE == SYSLOG_NG_JOURNALD_SYSTEM