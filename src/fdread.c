#include "fdread.h"
#include "messages.h"

static size_t
fd_do_read(FDRead *self, void *buf, size_t buflen, GSockAddr **sa)
{
  if ((self->flags & FR_RECV) == 0)
    {
      *sa = NULL;
      return read(self->fd, buf, buflen);
    }
  else 
    {
      struct sockaddr_storage sas;
      socklen_t salen = sizeof(sa);
      gint rc;

      rc = recvfrom(self->fd, buf, buflen, 0, 
		    (struct sockaddr *) &sas, &salen);
      if (rc != -1)
        (*sa) = g_sockaddr_new((struct sockaddr *) &sas, salen);
      return rc;
    }
}

FDRead *
fd_read_new(gint fd, guint flags)
{
  FDRead *self = g_new0(FDRead, 1);
  
  self->fd = fd;
  self->cond = G_IO_IN;
  self->read = fd_do_read;
  self->flags = flags;
  return self;
}

void
fd_read_free(FDRead *self)
{
  if ((self->flags & FR_DONTCLOSE) == 0)
    {
      msg_verbose("Closing log reader fd", 
                  evt_tag_int(EVT_TAG_FD, self->fd), 
                  NULL);
      close(self->fd);
    }
  g_free(self);
}
