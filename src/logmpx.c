#include "logmpx.h"


void
log_multiplexer_add_next_hop(LogMultiplexer *self, LogPipe *next_hop)
{
  /* NOTE: this flag is currently unused. it'll be used to tell whether we can process the multiplexed pipes in parallel threads. */
  if (next_hop->flags & (PIF_FINAL + PIF_FALLBACK))
    self->super.flags &= PIF_MPX_INDEP_PATHS;
  g_ptr_array_add(self->next_hops, next_hop);
}

static gboolean
log_multiplexer_init(LogPipe *s)
{
  LogMultiplexer *self = (LogMultiplexer *) s;
  gint i;
  
  for (i = 0; i < self->next_hops->len; i++)
    {
      LogPipe *next_hop = g_ptr_array_index(self->next_hops, i);
          
      if ((next_hop->flags & PIF_FALLBACK) != 0)
        {
          self->fallback_exists = TRUE;
        }
    }
  return TRUE;
}

static gboolean 
log_multiplexer_deinit(LogPipe *self)
{
  return TRUE;
}

static void
log_multiplexer_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogMultiplexer *self = (LogMultiplexer *) s;
  gint i;
  LogPathOptions local_options = LOG_PATH_OPTIONS_INIT;
  gboolean matched;
  gboolean delivered = FALSE;
  gboolean last_delivery;
  gint fallback;
  
  local_options.matched = &matched;
  
  for (fallback = 0; (fallback == 0) || (fallback == 1 && self->fallback_exists && !delivered); fallback++)
    {
      for (i = 0; i < self->next_hops->len; i++)
        {
          LogPipe *next_hop = g_ptr_array_index(self->next_hops, i);
          
          if (G_UNLIKELY(fallback == 0 && (next_hop->flags & PIF_FALLBACK) != 0))
            {
              continue;
            }
          else if (G_UNLIKELY(fallback && (next_hop->flags & PIF_FALLBACK) == 0))
            {
              continue;
            }
          
          if (self->super.flags & PIF_MPX_FLOW_CTRL_BARRIER)
            local_options.flow_control = (next_hop->flags & PIF_FLOW_CONTROL) ? TRUE : FALSE;
          else
            local_options.flow_control = path_options->flow_control;
            
          matched = TRUE;
          log_msg_add_ack(msg, &local_options);
          
          /* NOTE: this variable indicates that the upcoming message
           * delivery is the last one, thus we don't need to clone the
           * message, a simple ref is enough */
                    
          last_delivery = (self->super.pipe_next == NULL) && (i == self->next_hops->len - 1) && (!self->fallback_exists || delivered || fallback == 1);

          if (G_UNLIKELY(!last_delivery && (next_hop->flags & PIF_CLONE))) 
            log_pipe_queue(next_hop, log_msg_clone_cow(msg, &local_options), &local_options);
          else 
            log_pipe_queue(next_hop, log_msg_ref(msg), &local_options);
          
          if (matched)
            {
              delivered = TRUE; 
              if (G_UNLIKELY(next_hop->flags & PIF_FINAL))
                break;
            }
        }
    }
  log_pipe_forward_msg(s, msg, path_options);
}

static void
log_multiplexer_free(LogPipe *s)
{
  LogMultiplexer *self = (LogMultiplexer *) s;

  g_ptr_array_free(self->next_hops, TRUE);
  log_pipe_free(s);
}

LogMultiplexer *
log_multiplexer_new(guint32 flags)
{
  LogMultiplexer *self = g_new0(LogMultiplexer, 1);
  
  log_pipe_init_instance(&self->super);
  self->super.init = log_multiplexer_init;
  self->super.deinit = log_multiplexer_deinit;
  self->super.queue = log_multiplexer_queue;
  self->super.free_fn = log_multiplexer_free;
  self->next_hops = g_ptr_array_new();
  self->super.flags = PIF_MPX_INDEP_PATHS | flags;
  return self;
}
