#include "logqueue.h"

void
log_queue_init_instance(LogQueue *self, const gchar *persist_name)
{
  self->ref_cnt = 1;
  self->persist_name = persist_name ? g_strdup(persist_name) : NULL;
  self->free_fn = log_queue_free_method;
}

void
log_queue_free_method(LogQueue *self)
{
  g_free(self->persist_name);
  g_free(self);
}

