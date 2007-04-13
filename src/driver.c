
#include "driver.h"

void
log_drv_append(LogDriver *self, LogDriver *next)
{
  if (self->drv_next)
    log_drv_unref(self->drv_next);
  self->drv_next = log_drv_ref(next);
}

void 
log_drv_set_fifo_size(LogDriver *self, gint fifo_size)
{
}

void
log_drv_init_instance(LogDriver *s)
{
  log_pipe_init_instance(&s->super);
}

void
log_drv_free_instance(LogDriver *self)
{
  log_drv_unref(self->drv_next);
  self->drv_next = NULL;
}

