#include "sgroup.h"
#include "misc.h"
#include "messages.h"

#include <time.h>

static gboolean
log_source_group_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogSourceGroup *self = (LogSourceGroup *) s;
  LogDriver *p;

  self->chain_hostnames = cfg->chain_hostnames;
  self->use_dns = cfg->use_dns;
  self->use_fqdn = cfg->use_fqdn;
  /* self->cache = cfg->cache; */
  self->keep_hostname = cfg->keep_hostname;

  for (p = self->drivers; p; p = p->drv_next)
    {
      if (!p->super.init(&p->super, cfg, persist))
        {
          msg_error("Error initializing source driver",
                    evt_tag_str("source", self->name->str),
                    NULL);
	  return FALSE;
	}
      log_pipe_append(&p->super, s);
    }
  return TRUE;
}

static gboolean
log_source_group_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogSourceGroup *self = (LogSourceGroup *) s;
  LogDriver *p;

  for (p = self->drivers; p; p = p->drv_next)
    {
      if (!p->super.deinit(&p->super, cfg, persist))
        {
          msg_error("Error deinitializing source driver",
                    evt_tag_str("source", self->name->str),
                    NULL);
	  return FALSE;
	}
    }
  return TRUE;
}

static void
log_source_group_queue(LogPipe *s, LogMessage *msg, gint path_flags)
{
  LogSourceGroup *self = (LogSourceGroup *) s;
  
  msg->source_group = self;
  
  if (!self->keep_hostname || !msg->host->len) 
    {
      GString *name;

      name = resolve_hostname(msg->saddr, self->use_dns, self->use_fqdn);
      if (self->chain_hostnames) 
	{
	  if (msg->flags & LF_LOCAL) 
	    {
	      /* local */
	      g_string_sprintf(msg->host, "%s@%s", self->name->str, name->str);
	    }
	  else if (!msg->host->len) 
	    {
	      /* remote && no hostname */
	      g_string_sprintf(msg->host, "%s/%s", name->str, name->str);
	    } 
	  else 
	    {
	      /* everything else, append source hostname */
	      if (msg->host->len)
		g_string_sprintfa(msg->host, "/%s", name->str);
	      else
		g_string_assign(msg->host, name->str);
	    }
	}
      else 
	{
	  g_string_assign(msg->host, name->str);
	}

      g_string_free(name, TRUE);
    }

  if (!msg->host->len)
    {
      gchar buf[128];

      getshorthostname(buf, sizeof(buf));
      g_string_assign(msg->host, buf);
    }

  log_pipe_queue(self->super.pipe_next, msg, path_flags);
}

static void
log_source_group_free(LogPipe *s)
{
  LogSourceGroup *self = (LogSourceGroup *) s;
  
  log_drv_unref(self->drivers);
  g_string_free(self->name, TRUE);
  g_free(s);
}

LogSourceGroup *
log_source_group_new(gchar *name, LogDriver *drivers)
{
  LogSourceGroup *self = g_new0(LogSourceGroup, 1);

  log_pipe_init_instance(&self->super);  
  self->name = g_string_new(name);
  self->drivers = drivers;
  self->super.init = log_source_group_init;
  self->super.deinit = log_source_group_deinit;
  self->super.queue = log_source_group_queue;
  self->super.free_fn = log_source_group_free;
  return self;
}
