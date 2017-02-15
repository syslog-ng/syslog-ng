/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 2013 Viktor Juhasz
 * Copyright (c) 2013 Viktor Tusa
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
#include "persistable-state-presenter.h"
#include "logmsg/logmsg.h"
#include "str-format.h"

static GHashTable *persist_state_storage = NULL;

PersistableStatePresenterConstructFunc
persistable_state_presenter_get_constructor_by_prefix(const gchar *prefix)
{
  PersistableStatePresenterConstructFunc result = NULL;
  if (persist_state_storage)
    {
      result = g_hash_table_lookup(persist_state_storage, prefix);
    }
  return result;
}

void
persistable_state_presenter_register_constructor(const gchar *prefix,
                                                 PersistableStatePresenterConstructFunc handler)
{
  if (!persist_state_storage)
    {
      persist_state_storage = g_hash_table_new(g_str_hash, g_str_equal);
    }
  g_hash_table_insert(persist_state_storage, (gpointer) prefix, handler);
}
