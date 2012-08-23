/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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
  
#include "misc.h"
#include "dnscache.h"
#include "messages.h"
#include "gprocess.h"

#include <sys/types.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pwd.h>
#include <grp.h>
#endif /* _WIN32 */

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

static gchar local_hostname_fqdn[256];
static gchar local_hostname_short[256];

GString *
g_string_assign_len(GString *s, const gchar *val, gint len)
{
  g_string_truncate(s, 0);
  if (val && len)
    g_string_append_len(s, val, len);
  return s;
}

void
g_string_steal(GString *s)
{
  s->str = g_malloc0(1);
  s->allocated_len = 1;
  s->len = 0;
}

void
reset_cached_hostname(void)
{
  gchar *s;
  
  gethostname(local_hostname_fqdn, sizeof(local_hostname_fqdn) - 1);
  local_hostname_fqdn[sizeof(local_hostname_fqdn) - 1] = '\0';
  if (strchr(local_hostname_fqdn, '.') == NULL)
    {
      /* not fully qualified, resolve it using DNS or /etc/hosts */
      struct hostent *result = gethostbyname(local_hostname_fqdn);
      if (result)
        {
          strncpy(local_hostname_fqdn, result->h_name, sizeof(local_hostname_fqdn) - 1);
          local_hostname_fqdn[sizeof(local_hostname_fqdn) - 1] = '\0';
        }
    }
  /* NOTE: they are the same size, they'll fit */
  strcpy(local_hostname_short, local_hostname_fqdn);
  s = strchr(local_hostname_short, '.');
  if (s != NULL)
    *s = '\0';
}

void
getlonghostname(gchar *buf, gsize buflen)
{
  if (!local_hostname_fqdn[0])
    reset_cached_hostname();
  strncpy(buf, local_hostname_fqdn, buflen);
  buf[buflen - 1] = 0;
}

gboolean
resolve_hostname(GSockAddr **addr, gchar *name)
{
  if (addr)
    { 
#if HAVE_GETADDRINFO
      struct addrinfo hints;
      struct addrinfo *res;

      memset(&hints, 0, sizeof(hints));
      hints.ai_family = (*addr)->sa.sa_family;
      hints.ai_socktype = 0;
      hints.ai_protocol = 0;
      
      if (getaddrinfo(name, NULL, &hints, &res) == 0)
        {
          /* we only use the first entry in the returned list */
          switch ((*addr)->sa.sa_family)
            {
            case AF_INET:
              g_sockaddr_inet_set_address((*addr), ((struct sockaddr_in *) res->ai_addr)->sin_addr);
              break;
#if ENABLE_IPV6
            case AF_INET6:
              {
                guint16 port;

                /* we need to copy the whole sockaddr_in6 structure as it
                 * might contain scope and other required data */
                port = g_sockaddr_inet6_get_port(*addr);
                *g_sockaddr_inet6_get_sa(*addr) = *((struct sockaddr_in6 *) res->ai_addr);

                /* we need to restore the port number as it is zeroed out by the previous assignment */
                g_sockaddr_inet6_set_port(*addr, port);
                break;
              }
#endif
            default: 
              g_assert_not_reached();
              break;
            }
          freeaddrinfo(res);
        }
      else
        {
          msg_error("Error resolving hostname", evt_tag_str("host", name), NULL);
          return FALSE;
        }
#else
      struct hostent *he;
      
      he = gethostbyname(name);
      if (he)
        {
          switch ((*addr)->sa.sa_family)
            {
            case AF_INET:
              g_sockaddr_inet_set_address((*addr), *(struct in_addr *) he->h_addr);
              break;
            default: 
              g_assert_not_reached();
              break;
            }
          
        }
      else
        {
          msg_error("Error resolving hostname", evt_tag_str("host", name), NULL);
          return FALSE;
        }
#endif
    }
  return TRUE;
}

void
resolve_sockaddr(gchar *result, gsize *result_len, GSockAddr *saddr, gboolean usedns, gboolean usefqdn, gboolean use_dns_cache, gboolean normalize_hostnames)
{
  gchar *hname;
  gboolean positive;
  gchar *p, buf[256];
 
  if (saddr && saddr->sa.sa_family != AF_UNIX)
    {
      if (saddr->sa.sa_family == AF_INET
#if ENABLE_IPV6
          || saddr->sa.sa_family == AF_INET6
#endif
         )
        {
          void *addr;
          socklen_t addr_len;
          
          if (saddr->sa.sa_family == AF_INET)
            {
              addr = &((struct sockaddr_in *) &saddr->sa)->sin_addr;
              addr_len = sizeof(struct in_addr);
            }
#if ENABLE_IPV6
          else
            {
              addr = &((struct sockaddr_in6 *) &saddr->sa)->sin6_addr;
              addr_len = sizeof(struct in6_addr);
            }
#endif

          hname = NULL;
          if (usedns)
            {
              if ((!use_dns_cache || !dns_cache_lookup(saddr->sa.sa_family, addr, (const gchar **) &hname, &positive)) && usedns != 2)
                {
                  struct hostent *hp;
                      
                  hp = gethostbyaddr(addr, addr_len, saddr->sa.sa_family);
                  hname = (hp && hp->h_name) ? hp->h_name : NULL;

                  if (hname)
                    positive = TRUE;

                  if (use_dns_cache && hname)
                    {
                      /* resolution success, store this as a positive match in the cache */
                      dns_cache_store(FALSE, saddr->sa.sa_family, addr, hname, TRUE);
                    }
                } 
            }
            
          if (!hname) 
            {
              inet_ntop(saddr->sa.sa_family, addr, buf, sizeof(buf));
              hname = buf;
              if (use_dns_cache)
                dns_cache_store(FALSE, saddr->sa.sa_family, addr, hname, FALSE);
            }
          else 
            {
              if (!usefqdn && positive)
                {
                  /* we only truncate hostnames if they were positive
                   * matches (e.g. real hostnames and not IP
                   * addresses) */

                  p = strchr(hname, '.');

                  if (p)
                    {
                      if (p - hname > sizeof(buf))
                        p = &hname[sizeof(buf)] - 1;
                      memcpy(buf, hname, p - hname);
                      buf[p - hname] = 0;
                      hname = buf;
                    }
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
      if (!local_hostname_fqdn[0])
        reset_cached_hostname();
      if (usefqdn)
        {
          /* avoid copy */
          hname = local_hostname_fqdn;
        }
      else
        {
          hname = local_hostname_short;
        }
    }
  if (normalize_hostnames)
    {
      gint i;

      for (i = 0; hname[i] && i < ((*result_len) - 1); i++)
        {
          result[i] = g_ascii_tolower(hname[i]);
        }
      result[i] = '\0'; /* the closing \0 is not copied by the previous loop */
      *result_len = i;
    }
  else
    {
      gsize len = strlen(hname);

      if (*result_len < len - 1)
        len = *result_len - 1;
      memcpy(result, hname, len);
      result[len] = 0;
      *result_len = len;
    }
}

#ifndef _WIN32
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
resolve_user(const char *user, gint *uid)
{
  struct passwd *pw;
  gchar *endptr;

  *uid = 0;
  if (!(*user))
    return FALSE;

  *uid = strtol(user, &endptr, 0);
  if (*endptr)
    {
      pw = getpwnam(user);
      if (!pw)
        return FALSE;

      *uid = pw->pw_uid;
    }
  return TRUE;
}

gboolean 
resolve_group(const char *group, gint *gid)
{
  struct group *gr;
  gchar *endptr;

  *gid = 0;
  if (!(*group))
    return FALSE;
    
  *gid = strtol(group, &endptr, 0);
  if (*endptr)
    {
      gr = getgrnam(group);
      if (!gr)
        return FALSE;

      *gid = gr->gr_gid;
    }
  return TRUE;
}

gboolean 
resolve_user_group(char *arg, gint *uid, gint *gid)
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
#else

gboolean g_fd_set_nonblock(int fd, gboolean enable){ return TRUE; }
gboolean resolve_user_group(char *arg, gint *uid, gint *gid){ return TRUE; }
gboolean resolve_group(const char *group, gint *gid){ return TRUE; }
gboolean resolve_user(const char *user, gint *uid){ return TRUE; }
gboolean g_fd_set_cloexec(int fd, gboolean enable){ return TRUE; }

gchar *
wide_to_utf8(LPCWSTR str)
{
  int byte_num;
  // conversion from 'size_t' to 'int', possible loss of data
  int str_length  = -1;
  gchar *result;
  byte_num = WideCharToMultiByte(CP_UTF8, 0, str, str_length, NULL, 0, NULL,NULL);
  if (byte_num == 0)
    {
      msg_error("WideToUTF8 conversion failed", evt_tag_errno("error",GetLastError()),NULL);
      return NULL;
    }
  else
    {
      result = g_malloc(byte_num);
      byte_num = WideCharToMultiByte(CP_UTF8, 0, str, str_length, result, byte_num, NULL,NULL);
      if (byte_num == 0)
        {
          msg_error("WideToUTF8 final conversion failed",evt_tag_errno("error",GetLastError()),NULL);
          g_free(result);
          return NULL;
        }
    }
  return result;
}

LPWSTR
utf8_to_wide(const gchar *str)
{
  LPWSTR result = NULL;
  int size = 0;
  if (!(size = MultiByteToWideChar(CP_UTF8, 0, str, -1, 0, 0)))
    {
      return NULL;
    }

  result = g_malloc0(sizeof(WCHAR) *size);
  if (result == NULL)
    {
      return NULL;
    }

  if(!MultiByteToWideChar(CP_UTF8, 0, str, -1, result, size))
    {
      g_free(result);
      return NULL;
    }

  return result;
}

#endif /* _WIN32 */

/**
 *
 * This function receives a complete path (directory + filename) and creates
 * the directory portion if it does not exist. The point is that the caller
 * wants to ensure that the given filename can be opened after this function
 * returns. (at least it won't fail because of missing directories).
 **/
gboolean
create_containing_directory(gchar *name, gint dir_uid, gint dir_gid, gint dir_mode)
{
  gchar *dirname;
  struct stat st;
  gint rc;
  gchar *p;
  cap_t saved_caps;
  
  /* check that the directory exists */
  dirname = g_path_get_dirname(name);
  rc = stat(dirname, &st);
  g_free(dirname);
  
  if (rc == 0)
    {
      /* directory already exists */
      return TRUE;
    }
  else if (rc < 0 && errno != ENOENT)
    {
      /* some real error occurred */
      return FALSE;
    }
    
  /* directory does not exist */
  p = name + 1;
  
  p = strchr(p, '/');
  while (p) 
    {
      *p = 0;
      if (stat(name, &st) == 0) 
        {
          if (!S_ISDIR(st.st_mode))
            return FALSE;
        }
      else if (errno == ENOENT) 
        {
          if (g_mkdir(name, dir_mode < 0 ? 0700 : (mode_t) dir_mode) == -1)
            return FALSE;
          saved_caps = g_process_cap_save();
          g_process_cap_modify(CAP_CHOWN, TRUE);
          g_process_cap_modify(CAP_FOWNER, TRUE);
          set_permissions(name, dir_uid, dir_gid, dir_mode);
          g_process_cap_restore(saved_caps);
        }
      *p = '/';
      p = strchr(p + 1, '/');
    }
  return TRUE;
}

gchar *
find_file_in_path(const gchar *path, const gchar *filename, GFileTest test)
{
  gchar **dirs;
  gchar *fullname = NULL;
  gint i;

  if (filename[0] == '/' || !path)
    {
      if (g_file_test(filename, test))
        return g_strdup(filename);
      return NULL;
    }

  dirs = g_strsplit(path, G_SEARCHPATH_SEPARATOR_S, 0);
  i = 0;
  while (dirs && dirs[i])
    {
      fullname = g_build_filename(dirs[i], filename, NULL);
      if (g_file_test(fullname, test))
        break;
      g_free(fullname);
      fullname = NULL;
      i++;
    }
  g_strfreev(dirs);
  return fullname;
}

gchar *
format_hex_string(gpointer data, gsize data_len, gchar *result, gsize result_len)
{
  gint i;
  gint pos = 0;
  guchar *str = (guchar *) data;

  for (i = 0; i < data_len && result_len - pos > 2; i++)
    {
      g_snprintf(result + pos, 3, "%02x", str[i]);
      pos += 2;
    }   
  return result;
}

/**
 * Find CR or LF characters in the log message.
 *
 * It uses an algorithm similar to what there's in libc memchr/strchr.
 **/
gchar *
find_cr_or_lf(gchar *s, gsize n)
{
  gchar *char_ptr;
  gulong *longword_ptr;
  gulong longword, magic_bits, cr_charmask, lf_charmask;
  const char CR = '\r';
  const char LF = '\n';

  /* align input to long boundary */
  for (char_ptr = s; n > 0 && ((gulong) char_ptr & (sizeof(longword) - 1)) != 0; ++char_ptr, n--)
    {
      if (*char_ptr == CR || *char_ptr == LF)
        return char_ptr;
      else if (*char_ptr == 0)
        return NULL;
    }
    
  longword_ptr = (gulong *) char_ptr;

#if GLIB_SIZEOF_LONG == 8
  magic_bits = 0x7efefefefefefeffL;
#elif GLIB_SIZEOF_LONG == 4
  magic_bits = 0x7efefeffL; 
#else
  #error "unknown architecture"
#endif
  memset(&cr_charmask, CR, sizeof(cr_charmask));
  memset(&lf_charmask, LF, sizeof(lf_charmask));
    
  while (n > sizeof(longword))
    {
      longword = *longword_ptr++;
      if ((((longword + magic_bits) ^ ~longword) & ~magic_bits) != 0 ||
          ((((longword ^ cr_charmask) + magic_bits) ^ ~(longword ^ cr_charmask)) & ~magic_bits) != 0 || 
          ((((longword ^ lf_charmask) + magic_bits) ^ ~(longword ^ lf_charmask)) & ~magic_bits) != 0)
        {
          gint i;

          char_ptr = (gchar *) (longword_ptr - 1);
          
          for (i = 0; i < sizeof(longword); i++)
            {
              if (*char_ptr == CR || *char_ptr == LF)
                return char_ptr;
              else if (*char_ptr == 0)
                return NULL;
              char_ptr++;
            }
        }
      n -= sizeof(longword);
    }

  char_ptr = (gchar *) longword_ptr;

  while (n-- > 0)
    {
      if (*char_ptr == CR || *char_ptr == LF)
        return char_ptr;
      else if (*char_ptr == 0)
        return NULL;
      ++char_ptr;
    }

  return NULL;
}

GList *
string_array_to_list(const gchar *strlist[])
{
  gint i;
  GList *l = NULL;
  
  for (i = 0; strlist[i]; i++)
    {
      l = g_list_prepend(l, g_strdup(strlist[i]));
    }
  
  return g_list_reverse(l);
}

/*
 * NOTE: pointer values below 0x1000 (4096) are taken as special
 * values used by the application code and are not freed. Since this
 * is the NULL page, this should not cause memory leaks.
 */
void
string_list_free(GList *l)
{
  while (l)
    {
      /* some of the string lists use invalid pointer values as special
       * items, see SQL "default" item */

      if (GPOINTER_TO_UINT(l->data) > 4096)
        g_free(l->data);
      l = g_list_delete_link(l, l);
    }
}

typedef struct _WorkerThreadParams
{
  GThreadFunc func;
  gpointer data;
} WorkerThreadParams;

static gpointer
worker_thread_func(gpointer st)
{
  WorkerThreadParams *p = st;
  gpointer res;
  sigset_t mask;
  
  sigemptyset(&mask);
  sigaddset(&mask, SIGHUP);
  sigaddset(&mask, SIGCHLD);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGINT);
  sigprocmask(SIG_BLOCK, &mask, NULL);
  res = p->func(p->data);
  g_free(st);
  return res;
}

GThread *
create_worker_thread(GThreadFunc func, gpointer data, gboolean joinable, GError **error)
{
  GThread *h;
  WorkerThreadParams *p;
  
  p = g_new0(WorkerThreadParams, 1);
  p->func = func;
  p->data = data;
  
  h = g_thread_create_full(worker_thread_func, p, 128 * 1024, joinable, TRUE, G_THREAD_PRIORITY_NORMAL, error);
  if (!h)
    {
      g_free(p);
      return NULL;
    }
  return h;
}

gchar *
utf8_escape_string(const gchar *str, gssize len)
{
  int i;
  gchar *res, *res_pos;

  /* Check if string is a valid UTF-8 string */
  if (g_utf8_validate(str, -1, NULL))
      return g_strndup(str, len);

  /* It contains invalid UTF-8 sequences --> treat input as a
   * string containing binary data; escape those chars that have
   * no ASCII representation */
  res = g_new(gchar, 4 * len + 1);
  res_pos = res;

  for (i = 0; (i < len) && str[i]; i++)
    {
      if (g_ascii_isprint(str[i]))
        *(res_pos++) = str[i];
      else
        res_pos += sprintf(res_pos, "\\x%02x", ((guint8)str[i]));
    }

  *(res_pos++) = '\0';

  return res;
}


gint
set_permissions(gchar *name, gint uid, gint gid, gint mode)
{
  if (uid >= 0)
    if (chown(name, (uid_t) uid, -1)) return -1;
  if (gid >= 0)
    if (chown(name, -1, (gid_t) gid)) return -1;
  if (mode >= 0)
    if (chmod(name, (mode_t) mode)) return -1;
  return 0;
}

gint
set_permissions_fd(gint fd, gint uid, gint gid, gint mode)
{
  if (uid >= 0)
    if (fchown(fd, (uid_t) uid, -1)) return -1;
  if (gid >= 0)
    if (fchown(fd, -1, (gid_t) gid)) return -1;
  if (mode >= 0)
    if (fchmod(fd, (mode_t) mode)) return -1;
  return 0;
}

gchar *
normalize_option_name(gchar *name)
{
  gchar *p = name;
  while(*p != '\0')
    {
      if (*p == '-')
        *p = '_';
      p++;
    }
  return name;
}

char *
escape_windows_path(char *input)
{
  gchar *source_pointer = input;
  GString *result_string = g_string_sized_new(512);
  gchar *return_value;
  while(*source_pointer)
    {
      if (*source_pointer == '\\' ||
          *source_pointer == '"')
        {
          g_string_append_c(result_string,'\\');
        }
      g_string_append_c(result_string,*source_pointer);
      source_pointer++;
    }
  return_value = result_string->str;
  g_string_free(result_string,FALSE);
  return return_value;
}
