#ifndef FDWRITE_H_INCLUDED
#define FDWRITE_H_INCLUDED

#include "syslog-ng.h"
#include <unistd.h>

typedef struct _FDWrite
{
  gint fd;
  GIOCondition cond;
  size_t (*write)(struct _FDWrite *, const void *buf, size_t count);
} FDWrite;

static inline ssize_t 
fd_write(FDWrite *s, const void *buf, size_t count)
{
  return s->write(s, buf, count);
}

FDWrite *fd_write_new(gint fd);
void fd_write_free(FDWrite *self);


#endif
