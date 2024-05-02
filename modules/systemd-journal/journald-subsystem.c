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
#include "messages.h"

#include <glib.h>
#include <gmodule.h>

#if SYSLOG_NG_SYSTEMD_JOURNAL_MODE == SYSLOG_NG_JOURNALD_SYSTEM



#else

int (*sd_journal_open)(sd_journal **ret, int flags);
#if SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
int (*sd_journal_open_namespace)(sd_journal **ret, const char *namespace, int flags);
#endif
void (*sd_journal_close)(sd_journal *j);
int (*sd_journal_seek_head)(sd_journal *j);
int (*sd_journal_seek_tail)(sd_journal *j);
int (*sd_journal_get_cursor)(sd_journal *j, char **cursor);
int (*sd_journal_next)(sd_journal *j);
void (*sd_journal_restart_data)(sd_journal *j);
int (*sd_journal_enumerate_data)(sd_journal *j, const void **data, size_t *length);
int (*sd_journal_seek_cursor)(sd_journal *j, const char *cursor);
int (*sd_journal_test_cursor)(sd_journal *j, const char *cursor);
int (*sd_journal_get_fd)(sd_journal *j);
int (*sd_journal_process)(sd_journal *j);
int (*sd_journal_get_realtime_usec)(sd_journal *j, uint64_t *usec);
int (*sd_journal_get_realtime_usec)(sd_journal *j, uint64_t *usec);
int (*sd_journal_add_match)(sd_journal *j, const void *data, size_t size);
char *(*sd_id128_to_string)(sd_id128_t id, char s[SD_ID128_STRING_MAX]);
int (*sd_id128_get_boot)(sd_id128_t *ret);


#define LOAD_SYMBOL(library, symbol) _load_module_symbols(library, #symbol, (gpointer *)&symbol)

#define JOURNAL_LIBRARY_NAME "libsystemd-journal.so.0"
#define SYSTEMD_LIBRARY_NAME "libsystemd.so.0"
#define ID128_LIBRARY_NAME "libsystemd-id128.so.0"

const char *journald_libraries[] = {JOURNAL_LIBRARY_NAME, SYSTEMD_LIBRARY_NAME, NULL};
const char *journald_ext_libraries[] = {ID128_LIBRARY_NAME, JOURNAL_LIBRARY_NAME, SYSTEMD_LIBRARY_NAME, NULL};

static GModule *journald_module;
static GModule *journald_ext_module;

typedef struct sd_journal sd_journal;

static GModule *
_journald_module_open(const char *const *libraries)
{
  GModule *module = NULL;

  for (gint i = 0; (libraries[i] != NULL) && !module; i++)
    {
      module = g_module_open(libraries[i], 0);
      if (!module)
        msg_debug("No module by name", evt_tag_str("module", libraries[i]));
    }

  return module;
}

static gboolean
_load_module_symbols(GModule *library, gchar *symbol_name, gpointer *symbol)
{
  if (!g_module_symbol(library, symbol_name, symbol))
    {
      msg_error("Could not find symbol", evt_tag_str("symbol", symbol_name));
      return FALSE;
    }
  return TRUE;
}

static gboolean
_load_journald_symbols(void)
{
  if (!LOAD_SYMBOL(journald_module, sd_journal_open))
    return FALSE;

#if SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
  if (!LOAD_SYMBOL(journald_module, sd_journal_open_namespace))
    return FALSE;
#endif

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

  if (!LOAD_SYMBOL(journald_module, sd_journal_add_match))
    return FALSE;

  if (!LOAD_SYMBOL(journald_ext_module, sd_id128_to_string))
    return FALSE;

  if (!LOAD_SYMBOL(journald_ext_module, sd_id128_get_boot))
    return FALSE;

  return TRUE;
}

void
unload_journald_subsystem(void)
{
  if (journald_module)
    {
      g_module_close(journald_module);
      journald_module = NULL;
    }

  if (journald_ext_module)
    {
      g_module_close(journald_ext_module);
      journald_module = NULL;
    }
}

int
load_journald_subsystem(void)
{
  if (!journald_module && !journald_ext_module)
    {
      journald_module = _journald_module_open(journald_libraries);
      if (!journald_module)
        {
          msg_error("Could not find journald_module");
          return FALSE;
        }

      journald_ext_module = _journald_module_open(journald_ext_libraries);
      if (!journald_ext_module)
        {
          msg_error("Could not find journald_ext_module");
          return FALSE;
        }

      if (!_load_journald_symbols())
        {
          unload_journald_subsystem();
          return FALSE;
        }
    }
  return TRUE;
}

#endif
