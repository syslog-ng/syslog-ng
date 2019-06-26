/*
 * Copyright (c) 2017 Balabit
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

#include "glib.h"

#ifndef SYSLOG_NG_HAVE_G_LIST_COPY_DEEP

/*
  Less efficient than the original implementation in glib 2.53.2 that
  I wanted to port back, because this version iterates through the
  list twice. Though the original version depends on the internal api
  of GList (for example in terms on memory allocation), so I felt
  safer to reduce the problem to use public glib api only: g_list_copy
  and iteration.
 */

GList *
g_list_copy_deep(GList     *list,
                 GCopyFunc  func,
                 gpointer   user_data)
{
  if (!list)
    return NULL;

  GList *new_list = g_list_copy(list);
  if (func)
    {
      GList *iter = new_list;
      while (iter != NULL)
        {
          iter->data = func(iter->data, user_data);
          iter = g_list_next(iter);
        }
    }

  return new_list;
}

#endif

#ifndef SYSLOG_NG_HAVE_G_QUEUE_FREE_FULL

void
g_queue_free_full(GQueue *queue, GDestroyNotify free_func)
{
  g_queue_foreach(queue, (GFunc) free_func, NULL);
  g_queue_free(queue);
}

#endif

#ifndef SYSLOG_NG_HAVE_G_LIST_FREE_FULL

void
g_list_free_full (GList *list, GDestroyNotify free_func)
{
  g_list_foreach (list, (GFunc) free_func, NULL);
  g_list_free (list);
}

#endif


#ifndef SYSLOG_NG_HAVE_G_PTR_ARRAY_FIND_WITH_EQUAL_FUNC
gboolean
g_ptr_array_find_with_equal_func (GPtrArray     *haystack,
                                  gconstpointer  needle,
                                  GEqualFunc     equal_func,
                                  guint         *index_)
{
  guint i;

  g_return_val_if_fail (haystack != NULL, FALSE);

  if (equal_func == NULL)
    equal_func = g_direct_equal;

  for (i = 0; i < haystack->len; i++)
    {
      if (equal_func (g_ptr_array_index (haystack, i), needle))
        {
          if (index_ != NULL)
            *index_ = i;
          return TRUE;
        }
    }

  return FALSE;
}
#endif
