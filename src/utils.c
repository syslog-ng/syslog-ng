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
