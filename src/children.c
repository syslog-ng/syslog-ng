/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "children.h"

typedef struct _ChildEntry
{
  pid_t pid;
  gpointer callback_data;
  GDestroyNotify callback_data_destroy;
  void (*exit_callback)(pid_t pid, int status, gpointer user_data);
} ChildEntry;

GHashTable *child_hash;

static void
child_manager_child_entry_free(ChildEntry *ce)
{
  ce->callback_data_destroy(ce->callback_data);
  g_free(ce);
}

void
child_manager_register(pid_t pid, void (*callback)(pid_t, int, gpointer), gpointer user_data, GDestroyNotify callback_data_destroy)
{
  ChildEntry *ce = g_new0(ChildEntry, 1);

  ce->pid = pid;
  ce->exit_callback = callback;
  ce->callback_data = user_data;
  ce->callback_data_destroy = callback_data_destroy;

  g_hash_table_insert(child_hash, &ce->pid, ce);
}

void
child_manager_sigchild(pid_t pid, int status)
{
  ChildEntry *ce;

  ce = g_hash_table_lookup(child_hash, &pid);
  if (ce)
    {
      ce->exit_callback(pid, status, ce->callback_data);
      g_hash_table_remove(child_hash, &pid);
    }
}

void
child_manager_init(void)
{
  child_hash = g_hash_table_new_full(g_int_hash, g_int_equal, NULL, (GDestroyNotify) child_manager_child_entry_free);
}

void
child_manager_deinit(void)
{
  g_hash_table_destroy(child_hash);
}
