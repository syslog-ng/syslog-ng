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

#ifndef MISC_H_INCLUDED
#define MISC_H_INCLUDED

#include "syslog-ng.h"
#include "gsockaddr.h"

#include <sys/types.h>
#include <sys/socket.h>

GString *g_string_assign_len(GString *s, gchar *val, gint len);

char *getlonghostname(char *buf, size_t buflen);
char *getshorthostname(char *buf, size_t buflen);
long get_local_timezone_ofs(void);
GString *resolve_hostname(GSockAddr *saddr, int usedns, int usefqdn);
gboolean g_fd_set_nonblock(int fd, gboolean enable);

gboolean resolve_user(char *user, uid_t *uid);
gboolean resolve_group(char *group, gid_t *gid);
gboolean resolve_user_group(char *arg, uid_t *uid, gid_t *gid);

#endif
