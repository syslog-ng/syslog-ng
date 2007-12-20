#include "logqueue.h"
#include "logpipe.h"

gboolean
log_queue_push_tail(LogQueue *self, LogMessage *msg, gint path_flags)
{
  if (self->q->length / 2 >= self->qsize)
    return FALSE;
  log_msg_ref(msg);
  g_queue_push_tail(self->q, msg);
  g_queue_push_tail(self->q, GUINT_TO_POINTER(0x80000000 | path_flags));
  path_flags = PF_FLOW_CTL_OFF;
  log_msg_ack(msg, path_flags);
  log_msg_unref(msg);
  return TRUE;

}


gboolean 
log_queue_pop_head(LogQueue *self, LogMessage **msg, gint *path_flags)
{
  if (log_queue_get_length(self) > 0)
    {
      *msg = g_queue_pop_head(self->q);
      *path_flags = GPOINTER_TO_UINT (g_queue_pop_head(self->q)) & ~0x80000000;
      return TRUE;
    }
  return FALSE;
}


LogQueue *
log_queue_new(gint qsize)
{
  LogQueue *self = g_new0(LogQueue, 1);
  
  self->q = g_queue_new();
  self->qsize = qsize;
  return self;
}

static void
log_queue_free_queue(GQueue *q)
{
  while (!g_queue_is_empty(q))
    {
      LogMessage *lm;
      gint path_flags;
      
      lm = g_queue_pop_head(q);
      path_flags = GPOINTER_TO_UINT (g_queue_pop_head(q)) & 0x7FFFFFFF;
      log_msg_ack(lm, path_flags);
      log_msg_unref(lm);
    }
  g_queue_free(q);
}

void
log_queue_free(LogQueue *self)
{
  log_queue_free_queue(self->q);
  g_free(self);
}


