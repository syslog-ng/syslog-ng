/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
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

#ifndef STR_UTILS_H_INCLUDED
#define STR_UTILS_H_INCLUDED 1

#include "syslog-ng.h"

/* functions that should be implemented by GLib but they aren't */
GString *g_string_assign_len(GString *s, const gchar *val, gint len);
void g_string_steal(GString *s);


/*
 * DO NOT USE THIS MACRO UNLESS STRICTLY NECESSARY FOR PERFORMANCE REASONS.
 *
 * This macro tries hard to zero-terminate the string without moving it to a
 * new buffer.
 *
 * It is expected to be used at sites where the length is known and where
 * the input string is expected to be properly NUL terminated but not always.
 *
 * The macro checks if the string is indeed NUL terminated and if it is it
 * just assigns the original pointer to dest.  If it's not, it allocates a
 * new string via g_alloca() so it is automatically freed when the current
 * scope exits.
 *
 * This does not work as an inline function as we use g_alloca() and that
 * would be freed when the inline function exits (e.g.  when it is in fact
 * not inlined).
 */
#define APPEND_ZERO(dest, value, value_len)	\
  do { \
    gchar *__buf; \
    if (G_UNLIKELY(value[value_len] != 0)) \
      { \
        /* value is NOT zero terminated */ \
        \
        __buf = g_alloca(value_len + 1); \
        memcpy(__buf, value, value_len); \
        __buf[value_len] = 0; \
      } \
    else \
      { \
        /* value is zero terminated */ \
        __buf = (gchar *) value; \
      } \
    dest = __buf; \
  } while (0)


#endif
