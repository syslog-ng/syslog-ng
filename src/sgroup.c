/*
 * Copyright (c) 2002-2007 BalaBit IT Ltd, Budapest, Hungary                    
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "sgroup.h"
#include "misc.h"
#include "messages.h"
#include "stats.h"

#include <time.h>

static const gchar *
log_source_group_format_stats_name(LogSourceGroup *self)
{
  static gchar stats_name[64];

  g_snprintf(stats_name, sizeof(stats_name), "source(%s)", self->name->str);
  
  return stats_name;

}

static gboolean
log_source_group_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogSourceGroup *self = (LogSourceGroup *) s;
  LogDriver *p;

  self->chain_hostnames = cfg->chain_hostnames;
  self->normalize_hostnames = cfg->normalize_hostnames;
  self->use_dns = cfg->use_dns;
  self->use_fqdn = cfg->use_fqdn;
  self->use_dns_cache = cfg->use_dns_cache;
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
  stats_register_counter(SC_TYPE_PROCESSED, log_source_group_format_stats_name(self), &self->processed_messages, FALSE);
  return TRUE;
}

static gboolean
log_source_group_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogSourceGroup *self = (LogSourceGroup *) s;
  LogDriver *p;

  stats_unregister_counter(SC_TYPE_PROCESSED, log_source_group_format_stats_name(self), &self->processed_messages);
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

  resolve_hostname(msg->host_from, msg->saddr, self->use_dns, self->use_fqdn, self->use_dns_cache);
  
  msg->source_group = self;
  
  if (!self->keep_hostname || !msg->host->len) 
    {
      if (self->chain_hostnames) 
	{
	  if (msg->flags & LF_LOCAL) 
	    {
	      /* local */
	      g_string_sprintf(msg->host, "%s@%s", self->name->str, msg->host_from->str);
	    }
	  else if (!msg->host->len) 
	    {
	      /* remote && no hostname */
	      g_string_sprintf(msg->host, "%s/%s", msg->host_from->str, msg->host_from->str);
	    } 
	  else 
	    {
	      /* everything else, append source hostname */
	      if (msg->host->len)
		g_string_sprintfa(msg->host, "/%s", msg->host_from->str);
	      else
		g_string_assign(msg->host, msg->host_from->str);
	    }
	}
      else 
	{
	  g_string_assign(msg->host, msg->host_from->str);
	}

    }

  if (!msg->host->len)
    {
      gchar buf[256];

      getshorthostname(buf, sizeof(buf));
      g_string_assign(msg->host, buf);
    }
  if (self->normalize_hostnames)
    {
      g_strdown(msg->host->str);
    }
  log_pipe_queue(self->super.pipe_next, msg, path_flags);
  (*self->processed_messages)++;
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
