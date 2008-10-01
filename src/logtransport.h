#ifndef LOGTRANSPORT_H_INCLUDED
#define LOGTRANSPORT_H_INCLUDED

#include "syslog-ng.h"
#include "gsockaddr.h"

#define LTF_DONTCLOSE 0x0001
#define LTF_FSYNC     0x0002
#define LTF_APPEND    0x0004
#define LTF_RECV      0x0008


typedef struct _LogTransport LogTransport;

struct _LogTransport
{
  gint fd;
  GIOCondition cond;
  guint flags;
  gint timeout;
  gssize (*read)(LogTransport *self, gpointer buf, gsize count, GSockAddr **sa);
  gssize (*write)(LogTransport *self, const gpointer buf, gsize count);
  void (*free_fn)(LogTransport *self);
};

static inline gssize 
log_transport_write(LogTransport *self, const gpointer buf, gsize count)
{
  return self->write(self, buf, count);
}

static inline gssize
log_transport_read(LogTransport *self, gpointer buf, gsize count, GSockAddr **sa)
{
  return self->read(self, buf, count, sa);
}

LogTransport *log_transport_plain_new(gint fd, guint flags);
void log_transport_free(LogTransport *s);
void log_transport_free_method(LogTransport *s);


#endif
