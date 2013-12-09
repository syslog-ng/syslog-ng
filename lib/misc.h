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
  
#ifndef MISC_H_INCLUDED
#define MISC_H_INCLUDED

#include "syslog-ng.h"
#include "gsockaddr.h"

#include <sys/types.h>
#include <sys/socket.h>

/* functions that should be implemented by GLib but they aren't */
GString *g_string_assign_len(GString *s, const gchar *val, gint len);
void g_string_steal(GString *s);

gboolean g_fd_set_nonblock(int fd, gboolean enable);
gboolean g_fd_set_cloexec(int fd, gboolean enable);

/* deliberately using gint here as the extremal values may not fit into uid_t/gid_t */
gboolean resolve_user(const char *user, gint *uid);
gboolean resolve_group(const char *group, gint *gid);
gboolean resolve_user_group(char *arg, gint *uid, gint *gid);

gchar *find_cr_or_lf(gchar *s, gsize n);

gchar *find_file_in_path(const gchar *path, const gchar *filename, GFileTest test);

static inline void
init_sequence_number(gint32 *seqnum)
{
  *seqnum = 1;
}

static inline void 
step_sequence_number(gint32 *seqnum)
{
  (*seqnum)++;
  if (*seqnum < 0)
    *seqnum = 1;
}

GList *string_array_to_list(const gchar *strlist[]);
void string_list_free(GList *l);

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

gchar *utf8_escape_string(const gchar *str, gssize len);

#endif
