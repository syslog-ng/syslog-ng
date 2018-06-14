/*
 * Copyright (c) 2002-2010 Balabit
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#include "children.h"

typedef struct _ChildEntry
{
  pid_t pid;
  gpointer callback_data;
  GDestroyNotify callback_data_destroy;
  void (*exit_callback)(pid_t pid, int status, gpointer user_data);
} ChildEntry;

GHashTable *child_hash;
static void (*sigchld_callback)(int);

static void
child_manager_child_entry_free(ChildEntry *ce)
{
  if (ce->callback_data_destroy)
    ce->callback_data_destroy(ce->callback_data);
  g_free(ce);
}

void
child_manager_register(pid_t pid, void (*callback)(pid_t, int, gpointer), gpointer user_data,
                       GDestroyNotify callback_data_destroy)
{
  ChildEntry *ce = g_new0(ChildEntry, 1);

  ce->pid = pid;
  ce->exit_callback = callback;
  ce->callback_data = user_data;
  ce->callback_data_destroy = callback_data_destroy;

  g_hash_table_insert(child_hash, &ce->pid, ce);
}

void
child_manager_unregister(pid_t pid)
{
  if (g_hash_table_lookup(child_hash, &pid))
    {
      g_hash_table_remove(child_hash, &pid);
    }
}

void
child_manager_register_external_sigchld_handler(void (*callback)(int))
{
  sigchld_callback = callback;
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

  if (sigchld_callback)
    sigchld_callback(SIGCHLD);
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
