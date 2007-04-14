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
#include "dnscache.h"

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
#include <stdio.h>

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
  buf[bufsize - 1] = '\0';
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

int
format_zone_info(gchar *buf, size_t buflen, glong gmtoff)
{
  return snprintf(buf, buflen, "%c%02ld:%02ld",
                          gmtoff < 0 ? '-' : '+',
                          (gmtoff < 0 ? -gmtoff : gmtoff) / 3600,
                          (gmtoff % 3600) / 60);
}

/**
 * get_local_timezone_ofs:
 * @when: time in UTC
 * 
 * Return the zone offset (measured in seconds) of @when expressed in local
 * time. The function also takes care about daylight saving.
 **/
long
get_local_timezone_ofs(time_t when)
{
  struct tm ltm;
  long tzoff;
  
#if HAVE_STRUCT_TM_TM_GMTOFF
  
  ltm = *localtime(&when);
  tzoff = ltm.tm_gmtoff;
#else
  struct tm *gtm;
  
  ltm = *localtime(&when);
  gtm = gmtime(&when);

  tzoff = (ltm.tm_hour - gtm->tm_hour) * 3600;
  tzoff += (ltm.tm_min - gtm->tm_min) * 60;
  tzoff += ltm.tm_sec - gtm->tm_sec;
  
  if (tzoff > 0 && (ltm.tm_year < gtm->tm_year || ltm.tm_mon < gtm->tm_mon || ltm.tm_mday < gtm->tm_mday))
    tzoff -= 86400;
  else if (tzoff < 0 && (ltm.tm_year > gtm->tm_year || ltm.tm_mon > gtm->tm_mon || ltm.tm_mday > gtm->tm_mday))
    tzoff += 86400;
  
    
#endif

  return tzoff;
}

gboolean
resolve_hostname(GString *result, GSockAddr *saddr, gboolean usedns, gboolean usefqdn, gboolean use_dns_cache)
{
  static gchar local_hostname[256] = "";
  const gchar *hname;
  gchar *p, buf[256];
 
  if (saddr && saddr->sa.sa_family != AF_UNIX)
    {
      if (saddr->sa.sa_family == AF_INET || saddr->sa.sa_family == AF_INET6)
        {
          void *addr;
          socklen_t addr_len;
          
          if (saddr->sa.sa_family == AF_INET)
            {
              addr = &((struct sockaddr_in *) &saddr->sa)->sin_addr;
              addr_len = sizeof(struct in_addr);
            }
          else
            {
              addr = &((struct sockaddr_in6 *) &saddr->sa)->sin6_addr;
              addr_len = sizeof(struct in6_addr);
            }

          hname = NULL;
          if (usedns)
            {
              if ((!use_dns_cache || !dns_cache_lookup(saddr->sa.sa_family, addr, &hname)) && usedns != 2) 
                {
                  struct hostent *hp;
                      
                  hp = gethostbyaddr(addr, addr_len, saddr->sa.sa_family);
                  hname = (hp && hp->h_name) ? hp->h_name : NULL;
                  
                  if (use_dns_cache)
                    dns_cache_store(FALSE, saddr->sa.sa_family, addr, hname);
                } 
            }
            
          if (!hname) 
            {
              inet_ntop(saddr->sa.sa_family, addr, buf, sizeof(buf));
              hname = buf;
            }
          else 
            {
              g_strlcpy(buf, hname, sizeof(buf));
              hname = buf;
              if (!usefqdn) 
                {
                  p = strchr(buf, '.');
                  if (p) *p = 0;
                }
            }
        }
      else
        {
          g_assert_not_reached();
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
  g_string_assign(result, hname);
  return TRUE;
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
g_fd_set_cloexec(int fd, gboolean enable)
{
  int flags;

  if ((flags = fcntl(fd, F_GETFD)) == -1)
    return FALSE;
  if (enable)
    flags |= FD_CLOEXEC;
  else
    flags &= ~FD_CLOEXEC;

  if (fcntl(fd, F_SETFD, flags) < 0)
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

