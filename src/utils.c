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

#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <utmp.h>

#include "utils.h"

#ifndef HAVE_INET_ATON
int inet_aton(const char *cp, struct in_addr *addr)
{
	addr->s_addr = inet_addr(cp);
	if (addr->s_addr == -1) 
		return 0;
	return 1;
}
#endif

#ifndef HAVE_GETUTENT

static int utent_fd = -1;

#ifndef _PATH_UTMP
#define _PATH_UTMP "/var/log/utmp"
#endif

struct utmp *getutent(void)
{
	static struct utmp ut;
	int rc;

	if (utent_fd == -1) {
		utent_fd = open(_PATH_UTMP, O_RDONLY | O_NOCTTY);
	}
	if (utent_fd == -1)
		return NULL;
	rc = read(utent_fd, &ut, sizeof(ut));
	if (rc <= 0) {
		close(utent_fd);
		utent_fd = -1;
		return NULL;
	}
	else {
		return &ut;
	}
}

void endutent(void)
{
	if (utent_fd != -1) {
		close(utent_fd);
		utent_fd = -1;
	}
}

#endif
