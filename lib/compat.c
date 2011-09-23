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

#include "compat.h"

#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#ifndef SSIZE_MAX
#define SSIZE_MAX SHRT_MAX
#endif

#ifdef _MSC_VER

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <locale.h>

#define _ctloc(x)       (_CurrentTimeLocale->x)
/*
 * We do not implement alternate representations. However, we always
 * check whether a given modifier is allowed for a certain conversion.
 */
#define ALT_E           0x01
#define ALT_O           0x02
#define LEGAL_ALT(x)    { if (alt_format & ~(x)) return NULL; }

#define TM_YEAR_BASE   1900
#define IsLeapYear(x)   ((x % 4 == 0) && (x % 100 != 0 || x % 400 == 0))

typedef struct
{
  const char *abday[7];
  const char *day[7];
  const char *abmon[12];
  const char *mon[12];
  const char *am_pm[2];
  const char *d_t_fmt;
  const char *d_fmt;
  const char *t_fmt;
  const char *t_fmt_ampm;
} _TimeLocale;

static const _TimeLocale _DefaultTimeLocale =
{
  {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat",
  },
  {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
    "Friday", "Saturday"
  },
  {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  },
  {
    "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December"
  },
  {
    "AM", "PM"
  },
  "%a %b %d %H:%M:%S %Y",
  "%m/%d/%y",
  "%H:%M:%S",
  "%I:%M:%S %p"
};

static const _TimeLocale *_CurrentTimeLocale = &_DefaultTimeLocale;
static const char gmt[4] = { "GMT" };

ssize_t
readv(int fd, const struct iovec *vector, int count)
{
  size_t byte_size = 0;
  int i = 0;
  char *buffer = NULL;
  size_t to_copy = 0;
  char *data = NULL;
  ssize_t byte_read = 0;

  for ( i = 0; i < count; ++i)
    {
      if (SSIZE_MAX - byte_size < vector[i].iov_len)
        {
          SetLastError(EINVAL);
	  return -1;
        }
      byte_size += vector[i].iov_len;
    }

  buffer = _malloca(byte_size);

  byte_read = read(fd, buffer, byte_size);

  if(byte_read < 0)
    return -1;

  to_copy = byte_read;
  data = buffer;

  for (i = 0; i < count; ++i)
    {
       size_t copy = min (vector[i].iov_len, to_copy);
       memcpy(vector[i].iov_base, data, copy);
       to_copy -= copy;
       data -= copy;

       if (to_copy == 0)
         break;
    }
  _freea(buffer);
  return byte_read;
}

ssize_t
writev(int fd, const struct iovec *vector, int count)
{
  size_t byte_size = 0;
  int i = 0;
  char *buffer = NULL;
  size_t to_copy = 0;
  char *ret = NULL;
  ssize_t bytes_written = 0;

  for ( i = 0; i < count; ++i)
    {
      if (SSIZE_MAX - byte_size < vector[i].iov_len)
        {
          SetLastError(EINVAL);
	  return -1;
        }
      byte_size += vector[i].iov_len;
    }

  buffer = _malloca(byte_size);

  to_copy = byte_size;
  ret = buffer;

  for (i = 0; i < count; ++i)
    {
       size_t copy = min (vector[i].iov_len, to_copy);
       ret = memcpy(ret, vector[i].iov_base, copy);
       to_copy -= copy;
       if (to_copy == 0)
         break;
    }

  bytes_written = write(fd, buffer, byte_size);
  _freea(buffer);
  return bytes_written;
}

void *
mmap(void *addr, size_t len, int prot, int flags,
       int fildes, off_t off)
{
  /*
   *    Win 32         		   POSIX
   * PAGE_READONLY		PROT_READ
   * PAGE_READWRITE		(PROT_READ | PROT_WRITE)
   * PAGE_NOACCESS		PROT_NONE
   * PAGE_EXECUTE		PROT_EXEC
   * PAGE_EXECUTE_READ		(PROT_EXEC |PROT_READ)
   * PAGE_EXECUTE_READWRITE	(PROT_EXEC | PROT_READ | PROT_WRITE)
   *
   *
   * CreateFileMapping http://msdn.microsoft.com/en-us/library/aa366537(VS.85).aspx
   *
   * mmap and CreateFileMapping http://www.ibm.com/developerworks/systems/library/es-MigratingWin32toLinux.html#N102D8
   *
   * HANDLE WINAPI CreateFileMapping(
   *   __in      HANDLE hFile,
   *   __in_opt  LPSECURITY_ATTRIBUTES lpAttributes,
   *   __in      DWORD flProtect,
   *   __in      DWORD dwMaximumSizeHigh,
   *   __in      DWORD dwMaximumSizeLow,
   *   __in_opt  LPCTSTR lpName
   * );
   *
   * LPVOID WINAPI MapViewOfFile(
   *   __in  HANDLE hFileMappingObject,
   *   __in  DWORD dwDesiredAccess,
   *   __in  DWORD dwFileOffsetHigh,
   *   __in  DWORD dwFileOffsetLow,
   *   __in  SIZE_T dwNumberOfBytesToMap
   *   );
   */

  LARGE_INTEGER offset;
  DWORD flProtect;
  DWORD dwDesiredAccess;
  HANDLE file_mapping;
  void *ret_addr;
  offset.QuadPart = off ;

  switch (prot)
   {
     case PROT_READ:
      flProtect = PAGE_READONLY;
      dwDesiredAccess = FILE_MAP_READ;
     break;
     case PROT_READ | PROT_WRITE:
      flProtect = PAGE_READWRITE;
      dwDesiredAccess = FILE_MAP_ALL_ACCESS;
     break;
     case PROT_NONE:
      flProtect = PAGE_NOACCESS;
     break;
     case PROT_EXEC:
      flProtect = PAGE_EXECUTE;
      dwDesiredAccess = FILE_MAP_EXECUTE;
     break;
     case PROT_EXEC | PROT_READ:
      flProtect = PAGE_EXECUTE_READ | FILE_MAP_EXECUTE;
     break;
     case PROT_EXEC | PROT_READ | PROT_WRITE:
      flProtect = PAGE_EXECUTE_READWRITE;
      dwDesiredAccess = FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE;
     break;
   };

  file_mapping = CreateFileMapping(_get_osfhandle(fildes), NULL, flProtect, 0, 0, NULL);

  if (file_mapping == NULL)
    return MAP_FAILED;

  ret_addr = MapViewOfFile(file_mapping, dwDesiredAccess, offset.HighPart, offset.LowPart, len);

  /*
   * Mapped views of a file mapping object maintain internal references to the object,
   * and a file mapping object does not close until all references to it are released.
   * Therefore, to fully close a file mapping object, an application must unmap all mapped
   * views of the file mapping object by calling UnmapViewOfFile and close the file mapping
   * object handle by calling CloseHandle. These functions can be called in any order.
   */

   CloseHandle(file_mapping);
   if (ret_addr)
     return ret_addr;
   else
     return MAP_FAILED;
}

int
munmap(void *addr, size_t len)
{
  return UnmapViewOfFile(addr);
}

int
madvise(void *addr, size_t len, int advice)
{
  return TRUE;
}

/* getpagesize for windows */
long
getpagesize (void)
{
    static long g_pagesize = 0;
    if (! g_pagesize) {
        SYSTEM_INFO system_info;
        GetSystemInfo (&system_info);
        g_pagesize = system_info.dwPageSize;
    }
    return g_pagesize;
}

const char *
inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{
  if (af == AF_INET)
    {
      struct sockaddr_in in;
       memset(&in, 0, sizeof(in));
       in.sin_family = AF_INET;
       memcpy(&in.sin_addr, src, sizeof(struct in_addr));
       getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in), dst, cnt, NULL, 0, NI_NUMERICHOST);
       return dst;
    }
  else if (af == AF_INET6)
    {
      struct sockaddr_in6 in;
      memset(&in, 0, sizeof(in));
      in.sin6_family = AF_INET6;
      memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
      getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in6), dst, cnt, NULL, 0, NI_NUMERICHOST);
     return dst;
   }
  return NULL;
}

int
inet_pton(int af, const char *src, void *dst)
{
  struct addrinfo hints, *res, *ressave;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = af;

  if (getaddrinfo(src, NULL, &hints, &res) != 0)
    {
      return -1;
    }

  ressave = res;

  while (res)
    {
      memcpy(dst, res->ai_addr, res->ai_addrlen);
      res = res->ai_next;
    }

  freeaddrinfo(ressave);
  return 0;
}

int
inet_aton(const char *cp, struct in_addr *dst)
{
  int s = 0;

  s = inet_addr(cp);
  if (s == INADDR_NONE)
    return 0;

  dst->s_addr = s;
  return 1;
}

static const u_char *
conv_num(const unsigned char *buf, int *dest, unsigned int llim, unsigned int ulim)
{
    unsigned int result = 0;
    unsigned char ch;
    unsigned int rulim = ulim;

    ch = *buf;
    if (ch < '0' || ch > '9')
      return NULL;

    do
      {
        result *= 10;
        result += ch - '0';
        rulim /= 10;
        ch = *++buf;
      }
    while ((result * 10 <= ulim) && rulim && ch >= '0' && ch <= '9');

    if (result < llim || result > ulim)
      return NULL;

    *dest = result;
    return buf;
}

static const u_char *
find_string(const u_char *bp, int *tgt, const char * const *n1, const char * const *n2, int c)
{
    int i;
    unsigned int len;

    for (; n1 != NULL; n1 = n2, n2 = NULL)
      {
        for (i = 0; i < c; i++, n1++)
	  {
            len = strlen(*n1);
            if (strncasecmp(*n1, (const char *)bp, len) == 0)
	      {
                *tgt = i;
                return bp + len;
              }
          }
      }

    return NULL;
}


char *
strptime(const char *buf, const char *fmt, struct tm *tm)
{
  unsigned char c;
  const unsigned char *bp;
  int alt_format, i, split_year = 0;
  const char *new_fmt;

  bp = (const u_char *)buf;

  while (bp != NULL && (c = *fmt++) != '\0')
    {
      alt_format = 0;
      i = 0;

      if (isspace(c))
        {
          while (isspace(*bp))
            bp++;
          continue;
        }

      if (c != '%')
        goto literal;
again:
        switch (c = *fmt++)
	  {
            case '%':
literal:
              if (c != *bp++)
                return NULL;
              LEGAL_ALT(0);
              continue;

            case 'E':
              LEGAL_ALT(0);
              alt_format |= ALT_E;
              goto again;

            case 'O':
              LEGAL_ALT(0);
              alt_format |= ALT_O;
              goto again;

            case 'c':
              new_fmt = _ctloc(d_t_fmt);
              goto recurse;

            case 'D':
              new_fmt = "%m/%d/%y";
              LEGAL_ALT(0);
              goto recurse;

            case 'F':
              new_fmt = "%Y-%m-%d";
              LEGAL_ALT(0);
              goto recurse;

            case 'R':
              new_fmt = "%H:%M";
              LEGAL_ALT(0);
              goto recurse;

            case 'r':
              new_fmt =_ctloc(t_fmt_ampm);
              LEGAL_ALT(0);
              goto recurse;

            case 'T':
              new_fmt = "%H:%M:%S";
              LEGAL_ALT(0);
              goto recurse;

	    case 'X':
              new_fmt =_ctloc(t_fmt);
              goto recurse;

            case 'x':
              new_fmt =_ctloc(d_fmt);
              recurse:
              bp = (const u_char *)strptime((const char *)bp, new_fmt, tm);
              LEGAL_ALT(ALT_E);
            continue;

            case 'A':
            case 'a':
              bp = find_string(bp, &tm->tm_wday, _ctloc(day), _ctloc(abday), 7);
              LEGAL_ALT(0);
            continue;

            case 'B':
            case 'b':
            case 'h':
              bp = find_string(bp, &tm->tm_mon, _ctloc(mon), _ctloc(abmon), 12);
              LEGAL_ALT(0);
            continue;

            case 'C':
              i = 20;
              bp = conv_num(bp, &i, 0, 99);

              i = i * 100 - TM_YEAR_BASE;
              if (split_year)
                i += tm->tm_year % 100;
              split_year = 1;
              tm->tm_year = i;
              LEGAL_ALT(ALT_E);
            continue;

            case 'd':
            case 'e':
              bp = conv_num(bp, &tm->tm_mday, 1, 31);
              LEGAL_ALT(ALT_O);
            continue;

            case 'k':
              LEGAL_ALT(0);
              /* FALLTHROUGH */
            case 'H':
              bp = conv_num(bp, &tm->tm_hour, 0, 23);
              LEGAL_ALT(ALT_O);
            continue;

            case 'l':
              LEGAL_ALT(0);
            case 'I':
              bp = conv_num(bp, &tm->tm_hour, 1, 12);
              if (tm->tm_hour == 12)
                tm->tm_hour = 0;
              LEGAL_ALT(ALT_O);
            continue;

            case 'j':
              i = 1;
              bp = conv_num(bp, &i, 1, 366);
              tm->tm_yday = i - 1;
              LEGAL_ALT(0);
            continue;

            case 'M':
              bp = conv_num(bp, &tm->tm_min, 0, 59);
              LEGAL_ALT(ALT_O);
            continue;

            case 'm':
              i = 1;
              bp = conv_num(bp, &i, 1, 12);
              tm->tm_mon = i - 1;
              LEGAL_ALT(ALT_O);
            continue;

            case 'p':
              bp = find_string(bp, &i, _ctloc(am_pm), NULL, 2);
              if (tm->tm_hour > 11)
                return NULL;
              tm->tm_hour += i * 12;
              LEGAL_ALT(0);
            continue;

            case 'S':
              bp = conv_num(bp, &tm->tm_sec, 0, 61);
              LEGAL_ALT(ALT_O);
            continue;

            case 'U':
            case 'W':
               bp = conv_num(bp, &i, 0, 53);
               LEGAL_ALT(ALT_O);
             continue;

             case 'w':
               bp = conv_num(bp, &tm->tm_wday, 0, 6);
               LEGAL_ALT(ALT_O);
             continue;

             case 'Y':
               i = TM_YEAR_BASE;
               bp = conv_num(bp, &i, 0, 9999);
               tm->tm_year = i - TM_YEAR_BASE;
               LEGAL_ALT(ALT_E);
             continue;

             case 'y':
               bp = conv_num(bp, &i, 0, 99);

               if (split_year)
	         {
                   i += (tm->tm_year / 100) * 100;
		 }
               else
	         {
                   split_year = 1;
                   if (i <= 68)
                     i = i + 2000 - TM_YEAR_BASE;
                   else
                     i = i + 1900 - TM_YEAR_BASE;
                 }
               tm->tm_year = i;
             continue;

            case 'Z':
              tzset();
              if (strncmp((const char *)bp, gmt, 3) == 0)
	        {
                  tm->tm_isdst = 0;
#ifdef TM_GMTOFF
                  tm->TM_GMTOFF = 0;
#endif
#ifdef TM_ZONE
                  tm->TM_ZONE = gmt;
#endif
                  bp += 3;
              }
            else
	      {
                const unsigned char *ep;

                ep = find_string(bp, &i, (const char * const *)tzname, NULL, 2);
                if (ep != NULL)
		  {
                    tm->tm_isdst = i;
#ifdef TM_GMTOFF
                    tm->TM_GMTOFF = -(timezone);
#endif
#ifdef TM_ZONE
                    tm->TM_ZONE = tzname[i];
#endif
                  }
                bp = ep;
              }
            continue;

            case 'n':
            case 't':
              while (isspace(*bp))
                bp++;
              LEGAL_ALT(0);
            continue;

            default:
              return NULL;
          }
    }
    return bp;
}
#endif /* _MSC_VER */

#if !HAVE_PREAD || HAVE_BROKEN_PREAD

ssize_t
bb__pread(int fd, void *buf, size_t count, off_t offset)
{
  ssize_t ret;
  off_t old_offset;

  old_offset = lseek(fd, 0, SEEK_CUR);
  if (old_offset == -1)
    return -1;

  if (lseek(fd, offset, SEEK_SET) < 0)
    return -1;

  ret = read(fd, buf, count);
  if (ret < 0)
    return -1;

  if (lseek(fd, old_offset, SEEK_SET) < 0)
    return -1;
  return ret;
}

ssize_t
bb__pwrite(int fd, const void *buf, size_t count, off_t offset)
{
  ssize_t ret;
  off_t old_offset;

  old_offset = lseek(fd, 0, SEEK_CUR);
  if (old_offset == -1)
    return -1;

  if (lseek(fd, offset, SEEK_SET) < 0)
    return -1;

  ret = write(fd, buf, count);
  if (ret < 0)
    return -1;

  if (lseek(fd, old_offset, SEEK_SET) < 0)
    return -1;
  return ret;
}
#endif /* !HAVE_PREAD || HAVE_BROKEN_PREAD */

#if !HAVE_STRCASESTR
char *
strcasestr(const char *haystack, const char *needle)
{
  char c;
  size_t len;

  if ((c = *needle++) != 0)
    {
      c = tolower((unsigned char) c);
      len = strlen(needle);

      do
        {
          for (; *haystack && tolower((unsigned char) *haystack) != c; haystack++)
            ;
          if (!(*haystack))
            return NULL;
          haystack++;
        }
      while (strncasecmp(haystack, needle, len) != 0);
      haystack--;
    }
  return (char *) haystack;
}
#endif

#if !HAVE_MEMRCHR
const void *
memrchr(const void *s, int c, size_t n)
{
  const unsigned char *p = (unsigned char *) s + n - 1;

  while (p >= (unsigned char *) s)
    {
      if (*p == c)
        return p;
      p--;
    }
  return NULL;
}
#endif

#ifdef _AIX
intmax_t __strtollmax(const char *__nptr, char **__endptr, int __base)
{
  return strtoll(__nptr, __endptr, __base);
}
#endif

#ifndef HAVE_CLOCK_GETTIME

#include <time.h>
#ifdef __APPLE__
#include <sys/time.h>
#endif
int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
  struct timeval tv;
  int ret = 0;

  ret = gettimeofday(&tv, NULL);
  tp->tv_sec = tv.tv_sec;
  tp->tv_nsec = tv.tv_usec * 1000;

  return ret;
}
#endif
