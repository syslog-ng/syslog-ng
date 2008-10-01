#include "compat.h"

#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

#if !HAVE_PREAD || HAVE_BROKEN_PREAD

ssize_t 
__pread(int fd, void *buf, size_t count, off_t offset)
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
__pwrite(int fd, const void *buf, size_t count, off_t offset)
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
