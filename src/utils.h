#ifndef __UTILS_H_INCLUDED
#define __UTILS_H_INCLUDED

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <utmp.h>

#ifndef HAVE_INET_ATON
int inet_aton(const char *cp, struct in_addr *addr);
#endif

#ifndef HAVE_GETUTENT
struct utmp *getutent(void);
void endutent(void);
#endif

#define get_flags(flags, mask, shift) ((flags & mask) >> shift)
#define set_flags(flags, mask, shift, value) ((flags & ~mask) | value << shift)

#endif
