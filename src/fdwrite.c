#include "fdwrite.h"
#include "messages.h"

static size_t
fd_do_write(FDWrite *self, const void *buf, size_t buflen)
{
  return write(self->fd, buf, buflen);
}

FDWrite *
fd_write_new(gint fd)
{
  FDWrite *self = g_new0(FDWrite, 1);
  
  self->fd = fd;
  self->write = fd_do_write;
  self->cond = G_IO_OUT;
  return self;
}

void
fd_write_free(FDWrite *self)
{
  msg_verbose("Closing log writer fd",
              evt_tag_int("fd", self->fd),
              NULL);
  close(self->fd);
  g_free(self);
}
