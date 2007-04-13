#ifndef DRIVER_H_INCLUDED
#define DRIVER_H_INCLUDED

#include "syslog-ng.h"
#include "logpipe.h"
#include "cfg.h"

typedef struct _LogDriver
{
  LogPipe super;
  struct _LogDriver *drv_next;
} LogDriver;


void log_drv_append(LogDriver *self, LogDriver *next);
void log_drv_set_next_hop(LogDriver *self, LogPipe *next_hop);
void log_drv_set_fifo_size(LogDriver *self, gint fifo_size);

void log_drv_init_instance(LogDriver *self);
void log_drv_free_instance(LogDriver *self);

static inline LogDriver *
log_drv_ref(LogDriver *s)
{
  return (LogDriver *) log_pipe_ref(&s->super);
}

static inline void
log_drv_unref(LogDriver *s)
{
  log_pipe_unref(&s->super);
}

#endif
