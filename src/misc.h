#ifndef MISC_H_INCLUDED
#define MISC_H_INCLUDED

#include "syslog-ng.h"
#include "gsockaddr.h"

#include <sys/types.h>
#include <sys/socket.h>

GString *g_string_assign_len(GString *s, gchar *val, gint len);

char *getshorthostname(char *buf, size_t buflen);
GString *resolve_hostname(GSockAddr *saddr, int usedns, int usefqdn);
gboolean g_fd_set_nonblock(int fd, gboolean enable);

gboolean resolve_user(char *user, uid_t *uid);
gboolean resolve_group(char *group, gid_t *gid);
gboolean resolve_user_group(char *arg, uid_t *uid, gid_t *gid);

#endif
