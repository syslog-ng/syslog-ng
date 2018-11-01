/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include "string-list.h"

/* The aim of this module is to support the grammar to parse string lists
 * into simple linked lists.  Items can contain special values denoted by
 * integers.  We probably could find a _much_ nicer solution than this, I am
 * just extracting it from misc.c to its separate module. -- Bazsi */

/*
 * NOTE: pointer values below 0x1000 (4096) are taken as special values used
 * by the application code and are not duplicated, but assumed to be literal
 * tokens.
 */
GList *
string_list_clone(GList *string_list)
{
  GList *cloned = NULL;
  GList *l;

  for (l = string_list; l; l = l->next)
    cloned = g_list_append(cloned, GPOINTER_TO_UINT(l->data) > 4096 ? g_strdup(l->data) : l->data);

  return cloned;
}

GList *
string_array_to_list(const gchar *strlist[])
{
  gint i;
  GList *l = NULL;

  for (i = 0; strlist[i]; i++)
    {
      l = g_list_prepend(l, g_strdup(strlist[i]));
    }

  return g_list_reverse(l);
}

/*
 * NOTE: pointer values below 0x1000 (4096) are taken as special
 * values used by the application code and are not freed. Since this
 * is the NULL page, this should not cause memory leaks.
 */
static void
_free_valid_str_pointers(gpointer data)
{
  /* some of the string lists use invalid pointer values as special
   * items, see SQL "default" item */
  if (GPOINTER_TO_UINT(data) > 4096)
    g_free(data);
}

void
string_list_free(GList *l)
{
  g_list_free_full(l, _free_valid_str_pointers);
}
