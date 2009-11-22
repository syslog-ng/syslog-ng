/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
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

#include "compat.h"

#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

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
