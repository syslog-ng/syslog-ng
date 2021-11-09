/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Gergely Nagy
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef _SYSLOG_NG_UNIX_CREDENTIALS_H
#define _SYSLOG_NG_UNIX_CREDENTIALS_H

#include <sys/types.h>
#include <sys/socket.h>

#include "syslog-ng.h"

#if defined(__linux__)
# if SYSLOG_NG_HAVE_STRUCT_UCRED && SYSLOG_NG_HAVE_CTRLBUF_IN_MSGHDR
# define CRED_PASS_SUPPORTED
# define cred_t struct ucred
# define cred_get(c,x) (c->x)
# endif
#elif defined(__FreeBSD__)
# if SYSLOG_NG_HAVE_STRUCT_CMSGCRED && SYSLOG_NG_HAVE_CTRLBUF_IN_MSGHDR
#  define CRED_PASS_SUPPORTED
#  define SCM_CREDENTIALS SCM_CREDS
#  define cred_t struct cmsgcred
#  define cred_get(c,x) (c->cmcred_##x)
# endif
#endif

void setsockopt_so_passcred(gint fd, gint enable);

#endif
