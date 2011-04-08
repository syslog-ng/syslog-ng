#include "logqueue.h"
#include "stats.h"

void
log_queue_set_counters(LogQueue *self, guint32 *stored_messages, guint32 *dropped_messages)
{
  self->stored_messages = stored_messages;
  self->dropped_messages = dropped_messages;
  stats_counter_set(self->stored_messages, log_queue_get_length(self));
}

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

