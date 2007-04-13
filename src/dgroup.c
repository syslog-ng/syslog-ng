#include "dgroup.h"
#include "misc.h"

#include <sys/time.h>

static gboolean
log_dest_group_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogDestGroup *self = (LogDestGroup *) s;
  LogDriver *p;

  for (p = self->drivers; p; p = p->drv_next)
    {
      if (!p->super.init(&p->super, cfg, persist))
	return FALSE;
    }
  return TRUE;
}

static gboolean
log_dest_group_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogDestGroup *self = (LogDestGroup *) s;
  LogDriver *p;

  for (p = self->drivers; p; p = p->drv_next)
    {
      if (!p->super.deinit(&p->super, cfg, persist))
	return FALSE;
    }
  return TRUE;
}

static void
log_dest_group_ack(LogMessage *msg, gpointer user_data)
{
  log_msg_ack_block_end(msg);
  log_msg_ack(msg);
  log_msg_unref(msg);
}

static void
log_dest_group_queue(LogPipe *s, LogMessage *msg, gint path_flags)
{
  LogDestGroup *self = (LogDestGroup *) s;
  LogDriver *p;
  
  if ((path_flags & PF_FLOW_CTL_OFF) == 0)
    {
      log_msg_ref(msg);
      log_msg_ack_block_start(msg, log_dest_group_ack, self);
    }
  for (p = self->drivers; p; p = p->drv_next)
    {
      if ((path_flags & PF_FLOW_CTL_OFF) == 0)
        log_msg_ack_block_inc(msg);
      log_pipe_queue(&p->super, msg, path_flags);
    }
  if ((path_flags & PF_FLOW_CTL_OFF) == 0)
    log_msg_ack(msg);
}

static void
log_dest_group_free(LogPipe *s)
{
  LogDestGroup *self = (LogDestGroup *) s;
  
  log_drv_unref(self->drivers);
  g_string_free(self->name, TRUE);
  g_free(s);
}

LogDestGroup *
log_dest_group_new(gchar *name, LogDriver *drivers)
{
  LogDestGroup *self = g_new0(LogDestGroup, 1);
  
  self->name = g_string_new(name);
  self->drivers = drivers;
  self->super.init = log_dest_group_init;
  self->super.deinit = log_dest_group_deinit;
  self->super.queue = log_dest_group_queue;
  self->super.free_fn = log_dest_group_free;
  return self;
}
