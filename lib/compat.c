/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2011 Balázs Scheidler
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
#include <stdlib.h>

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
#endif

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
