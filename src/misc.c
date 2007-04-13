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

#include "misc.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#include <time.h>

GString *
g_string_assign_len(GString *s, gchar *val, gint len)
{
  g_string_truncate(s, 0);
  if (val && len)
    g_string_append_len(s, val, len);
  return s;
}

char *
getshorthostname(char *buf, size_t bufsize)
{
  char *s;
  
  gethostname(buf, bufsize - 1);
  buf[bufsize - 1] = '\0';
  s = strchr(buf, '.');
  if (s != NULL)
    *s = '\0';
  return buf;
}

char *
getlonghostname(char *buf, size_t bufsize)
{
  gethostname(buf, bufsize - 1);
  if (strchr(buf, '.') == NULL)
    {
      struct hostent *result = gethostbyname(buf);
      if (result)
        {
          strncpy(buf, result->h_name, bufsize - 1);
          buf[bufsize - 1] = 0;
        }
    }
  return buf;
}

long
get_local_timezone_ofs(time_t when)
{
  struct tm *tm;
  
  tm = localtime(&when);
  return tm->tm_gmtoff;
#if 0
  timezone - (tm.tm_isdst > 0 ? 3600 : 0);
#endif
}

GString *
resolve_hostname(GSockAddr *saddr,
		 int usedns, int usefqdn)
{
  static gchar local_hostname[128] = "";
  char *hname, *p;
 
  if (saddr && saddr->sa.sa_family == AF_INET)
    {
      struct sockaddr_in *inet_addr = (struct sockaddr_in *) &saddr->sa;

      /* FIXME: add nscache support here */

      if (usedns) 
	{
	  struct hostent *hp;
	  
	  hp = gethostbyaddr((char *) &(inet_addr->sin_addr),
			     sizeof(struct in_addr), AF_INET);
	  hname = (hp && hp->h_name) ? hp->h_name : NULL;
	} 
      else
	hname = NULL;
      
      if (!hname) 
	{
	  hname = inet_ntoa(inet_addr->sin_addr);
	}
      else 
	{
	  if (!usefqdn) 
	    {
	      p = strchr(hname, '.');
	      if (p) *p = 0;
	    }
	}
    }
  else 
    {
      if (!local_hostname[0]) 
	{
          if (usefqdn)
            getlonghostname(local_hostname, sizeof(local_hostname));
          else
	    getshorthostname(local_hostname, sizeof(local_hostname));
	}

      hname = local_hostname;
    }
  return g_string_new(hname);
}

gboolean
g_fd_set_nonblock(int fd, gboolean enable)
{
  int flags;

  if ((flags = fcntl(fd, F_GETFL)) == -1)
    return FALSE;
  if (enable)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;

  if (fcntl(fd, F_SETFL, flags) < 0)
    {
      return FALSE;
    }
  return TRUE;
}

gboolean 
resolve_user(const char *user, uid_t *uid)
{
  struct passwd *pw;

  *uid = 0;
  pw = getpwnam(user);
  if (pw) 
    {
      *uid = pw->pw_uid;
    }
  else 
    {
      *uid = atoi(user);
      if (*uid == 0)
        return FALSE;
    }
  return TRUE;
}

gboolean 
resolve_group(const char *group, gid_t *gid)
{
  struct group *gr;

  *gid = 0;
  gr = getgrnam(group);
  if (gr) 
    {
      *gid = gr->gr_gid;
    }
  else 
    {
      *gid = atoi(group);
      if (*gid == 0)
        return FALSE;
    }
  return TRUE;
}

gboolean 
resolve_user_group(char *arg, uid_t *uid, gid_t *gid)
{
  char *user, *group;

  *uid = 0;
  user = strtok(arg, ":.");
  group = strtok(NULL, "");
  
  if (user && !resolve_user(user, uid))
    return FALSE;
  if (group && !resolve_group(group, gid))
    return FALSE;
  return TRUE;
}

