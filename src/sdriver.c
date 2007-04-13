#include "sdriver.h"

void 
log_source_driver_set_next_hop(LogSourceDriver *self, LogPipe *next_hop)
{
  self->next_hop = next_hop;
}

void
log_source_driver_init(LogSourceDriver *self)
{
  self->ref_cnt = 1;
}
