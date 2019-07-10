/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 2011 Ryan Lortie
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
#ifndef COMPAT_GLIB_H_INCLUDED
#define COMPAT_GLIB_H_INCLUDED 1

#include "compat/compat.h"

#define GLIB_DISABLE_DEPRECATION_WARNINGS 1

#include <glib.h>

#ifndef SYSLOG_NG_HAVE_G_LIST_COPY_DEEP
GList *g_list_copy_deep (GList *list, GCopyFunc func, gpointer user_data);
#endif

#ifndef g_atomic_pointer_add

/* This implementation was copied from glib 2.46, copyright Ryan Lortie
 * under the LGPL 2 or later, which is compatible with our LGPL 2.1 license.
 * NOTE: this code only runs if we are running on an old glib version (e.g.
 * older than 2.32)
 * */

#define g_atomic_pointer_add(atomic, val) \
  (G_GNUC_EXTENSION ({                                                       \
    G_STATIC_ASSERT (sizeof *(atomic) == sizeof (gpointer));                 \
    (void) (0 ? (gpointer) *(atomic) : 0);                                   \
    (void) (0 ? (val) ^ (val) : 0);                                          \
    (gssize) __sync_fetch_and_add ((atomic), (val));                         \
  }))
#define g_atomic_pointer_or(atomic, val) \
  (G_GNUC_EXTENSION ({                                                       \
    G_STATIC_ASSERT (sizeof *(atomic) == sizeof (gpointer));                 \
    (void) (0 ? (gpointer) *(atomic) : 0);                                   \
    (void) (0 ? (val) ^ (val) : 0);                                          \
    (gsize) __sync_fetch_and_or ((atomic), (val));                           \
  }))
#define g_atomic_pointer_xor(atomic, val) \
  (G_GNUC_EXTENSION ({                                                       \
    G_STATIC_ASSERT (sizeof *(atomic) == sizeof (gpointer));                 \
    (void) (0 ? (gpointer) *(atomic) : 0);                                   \
    (void) (0 ? (val) ^ (val) : 0);                                          \
    (gsize) __sync_fetch_and_xor ((atomic), (val));                          \
  }))
#define g_atomic_pointer_and(atomic, val) \
  (G_GNUC_EXTENSION ({                                                       \
    G_STATIC_ASSERT (sizeof *(atomic) == sizeof (gpointer));                 \
    (void) (0 ? (gpointer) *(atomic) : 0);                                   \
    (void) (0 ? (val) ^ (val) : 0);                                          \
    (gsize) __sync_fetch_and_and ((atomic), (val));                          \
  }))

#endif

#ifndef SYSLOG_NG_HAVE_G_QUEUE_FREE_FULL
void g_queue_free_full(GQueue *queue, GDestroyNotify free_func);
#endif

#ifndef SYSLOG_NG_HAVE_G_LIST_FREE_FULL
void g_list_free_full (GList *list, GDestroyNotify free_func);
#endif

#ifndef SYSLOG_NG_HAVE_G_PTR_ARRAY_FIND_WITH_EQUAL_FUNC
gboolean g_ptr_array_find_with_equal_func (GPtrArray *haystack,
                                           gconstpointer needle,
                                           GEqualFunc equal_func,
                                           guint *index_);
#endif

#endif
