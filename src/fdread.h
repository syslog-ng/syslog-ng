#ifndef FDREAD_H_INCLUDED
#define FDREAD_H_INCLUDED

#include "syslog-ng.h"
#include "gsockaddr.h"

#include <unistd.h>

#define FR_DONTCLOSE 0x0001
#define FR_RECV      0x0002

typedef struct _FDRead
{
  gint fd;
  GIOCondition cond;
  guint flags;
  size_t (*read)(struct _FDRead *, void *buf, size_t count, GSockAddr **sa);
} FDRead;

static inline ssize_t 
fd_read(FDRead *s, void *buf, size_t count, GSockAddr **sa)
{
  return s->read(s, buf, count, sa);
}

FDRead *fd_read_new(gint fd, guint flags);
void fd_read_free(FDRead *self);

#endif
