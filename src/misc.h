/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
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
  
#ifndef MISC_H_INCLUDED
#define MISC_H_INCLUDED

#include "syslog-ng.h"
#include "gsockaddr.h"

#include <sys/types.h>
#include <sys/socket.h>

GString *g_string_assign_len(GString *s, const gchar *val, gint len);

gboolean g_fd_set_nonblock(int fd, gboolean enable);
gboolean g_fd_set_cloexec(int fd, gboolean enable);

/* deliberately using gint here as the extremal values may not fit into uid_t/gid_t */
gboolean resolve_user(const char *user, gint *uid);
gboolean resolve_group(const char *group, gint *gid);
gboolean resolve_user_group(char *arg, gint *uid, gint *gid);

/* name resolution */
void reset_cached_hostname(void);
void getlonghostname(gchar *buf, gsize buflen);
gboolean resolve_sockaddr(gchar **result, GSockAddr *saddr, gboolean usedns, gboolean usefqdn, gboolean use_dns_cache, gboolean normalize_hostnames);
gboolean resolve_hostname(GSockAddr **addr, gchar *name);

gchar *format_hex_string(gpointer str, gsize str_len, gchar *result, gsize result_len);
gchar *find_cr_or_lf(gchar *s, gsize n);

gboolean create_containing_directory(gchar *name, gint dir_uid, gint dir_gid, gint dir_mode);
GThread *create_worker_thread(GThreadFunc func, gpointer data, gboolean joinable, GError **error);


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

#define APPEND_ZERO(value, value_len) \
  ({ \
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
  __buf; })


#endif
