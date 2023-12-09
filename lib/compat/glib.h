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

#include <glib.h>

#ifndef SYSLOG_NG_HAVE_G_LIST_COPY_DEEP
GList *g_list_copy_deep (GList *list, GCopyFunc func, gpointer user_data);
#endif

#ifndef SYSLOG_NG_HAVE_G_CANONICALIZE_FILENAME
gchar *g_canonicalize_filename (const gchar *filename,
                                const gchar *relative_to);
#endif

#ifndef SYSLOG_NG_HAVE_G_PTR_ARRAY_FIND_WITH_EQUAL_FUNC
gboolean g_ptr_array_find_with_equal_func (GPtrArray *haystack,
                                           gconstpointer needle,
                                           GEqualFunc equal_func,
                                           guint *index_);
#endif

#if !GLIB_CHECK_VERSION(2, 54, 0)
#define g_base64_encode g_base64_encode_fixed
gchar *g_base64_encode_fixed(const guchar *data, gsize len);
#endif

#if !GLIB_CHECK_VERSION(2, 40, 0)
#define g_hash_table_insert slng_g_hash_table_insert
gboolean slng_g_hash_table_insert (GHashTable *hash_table, gpointer key, gpointer value);
#endif

#if !GLIB_CHECK_VERSION(2, 64, 0)
#define g_utf8_get_char_validated g_utf8_get_char_validated_fixed
gunichar g_utf8_get_char_validated_fixed (const gchar *p, gssize max_len);
#endif

#if !GLIB_CHECK_VERSION(2, 68, 0)
#define g_memdup2 g_memdup
#endif

#if !GLIB_CHECK_VERSION(2, 70, 0)
#define g_pattern_spec_match_string g_pattern_match_string
#define g_pattern_spec_match g_pattern_match
#endif

#endif
