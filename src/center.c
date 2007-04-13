/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
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

#include "center.h"
#include "sgroup.h"
#include "dgroup.h"
#include "filter.h"
#include "messages.h"
#include "afinter.h"

#include <assert.h>

/**
 * log_endpoint_append:
 * @a: first LogEndpoint
 * @b: second LogEndpoint
 *
 * This function appends @b to @a in a linked list using the ep_next field
 * in LogEndpoint.
 **/
void
log_endpoint_append(LogEndpoint *a, LogEndpoint *b)
{
  a->ep_next = b;
}

/**
 * log_endpoint_new:
 * @type: specifies endpoint type one of EP_* 
 * @name: name of the endpoint which is used in the configuration as an identifier
 * 
 * This function constructs a LogEndpoint object encapsulating a
 * source/filter/destination node in a log statement.
 **/
LogEndpoint *
log_endpoint_new(gint type, gchar *name)
{
  LogEndpoint *self = g_new0(LogEndpoint, 1);
  
  self->type = type;
  self->name = g_string_new(name);
  return self;
}

/**
 * log_endpoint_free:
 * @self: LogEndpoint instance
 * 
 * This function frees a LogEndpoint instance, first by freeing the
 * referenced object stored in the @ref attribute then freeing @self.
 * NOTE: @ref is set when the given configuration is initialized.
 **/
void
log_endpoint_free(LogEndpoint *self)
{
  g_string_free(self->name, TRUE);
  if (self->ref)
    {
      switch (self->type)
        {
        case EP_SOURCE:
          log_sgrp_unref((LogSourceGroup *) self->ref);
          break;
        case EP_FILTER:
          log_filter_unref((LogFilterRule *) self->ref);
          break;
        case EP_DESTINATION:
          log_dgrp_unref((LogDestGroup *) self->ref);
          break;
        default:
          assert(0);
          break;
        }
    }
  g_free(self);
}

/**
 * log_connection_new:
 * @endpoints: list of endpoints in this log statement
 * @flags: a combination of LC_* flags as specified by the administrator
 *
 * This function constructs a LogConnection object which encapsulates a log
 * statement in the configuration, e.g. it has one or more sources, filters
 * and destinations each represented by a LogEndpoint object.
 **/
LogConnection *
log_connection_new(LogEndpoint *endpoints, guint32 flags)
{
  LogConnection *self = g_new0(LogConnection, 1);
  LogEndpoint *ep;
  
  self->flags = flags;
  self->source_cache = g_hash_table_new(g_str_hash, g_str_equal);
  self->sources = g_ptr_array_new();
  self->filters = g_ptr_array_new();
  self->destinations = g_ptr_array_new();
  for (ep = endpoints; ep; ep = ep->ep_next)
    {
      switch (ep->type)
        {
        case EP_SOURCE:
          g_ptr_array_add(self->sources, ep);
          g_hash_table_insert(self->source_cache, ep->name->str, ep);
          break;
        case EP_FILTER:
          g_ptr_array_add(self->filters, ep);
          break;
        case EP_DESTINATION:
          g_ptr_array_add(self->destinations, ep);
          break;
        default:
          g_assert(0);
          break;
        }
    }
  
  return self;
}

/**
 * log_connection_free:
 * @self: LogConnection instance
 *
 * This function frees the LogConnection object encapsulating a log
 * statement pointed to by @self.
 **/
void
log_connection_free(LogConnection *self)
{
  int i;
  
  for (i = 0; i < self->sources->len; i++)
    {
      log_endpoint_free(g_ptr_array_index(self->sources, i));
    }
  
  for (i = 0; i < self->filters->len; i++)
    {
      log_endpoint_free(g_ptr_array_index(self->filters, i));
    }
  
  for (i = 0; i < self->destinations->len; i++)
    {
      log_endpoint_free(g_ptr_array_index(self->destinations, i));
    }
  
  g_hash_table_destroy(self->source_cache);
  g_ptr_array_free(self->sources, TRUE);
  g_ptr_array_free(self->filters, TRUE);
  g_ptr_array_free(self->destinations, TRUE);  
  g_free(self);
}

static void
log_center_init_component(gchar *key, LogPipe *value, LogCenter *self)
{
  if (!self->success || value->init(value, self->cfg, self->persist) == FALSE)
    self->success = FALSE;
  if (self->state == LC_STATE_INIT_SOURCES)
    {
      log_pipe_append(value, &self->super);
    }
}

static void
log_center_deinit_component(gchar *key, LogPipe *value, LogCenter *self)
{
  if (!self->success || value->deinit(value, self->cfg, self->persist) == FALSE)
    self->success = FALSE;
}

static gboolean
log_center_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogCenter *self = (LogCenter *) s;
  gint i, j;
  
  self->cfg = cfg;
  self->persist = persist;
  
  /* resolve references within the configuration */
  
  for (i = 0; i < cfg->connections->len; i++)
    {
      LogConnection *conn = (LogConnection *) g_ptr_array_index(cfg->connections, i);
      LogEndpoint *ep;
      
      for (j = 0; j < conn->sources->len; j++)
        {
          ep = (LogEndpoint *) g_ptr_array_index(conn->sources, j);
          
          ep->ref = g_hash_table_lookup(cfg->sources, ep->name->str);
          if (!ep->ref)
            {
              msg_error("Error in configuration, unresolved source reference",
                        evt_tag_str("source", ep->name->str),
                        NULL);
              return FALSE;
            }
          log_sgrp_ref(ep->ref);
        }
      
      for (j = 0; j < conn->filters->len; j++)
        {
          ep = (LogEndpoint *) g_ptr_array_index(conn->filters, j);
          
          ep->ref = g_hash_table_lookup(cfg->filters, ep->name->str);
          if (!ep->ref)
            {
              msg_error("Error in configuration, unresolved filter reference",
                        evt_tag_str("filter", ep->name->str),
                        NULL);
              return FALSE;
            }
          log_filter_ref(ep->ref);
        }

      for (j = 0; j < conn->destinations->len; j++)
        {
          ep = (LogEndpoint *) g_ptr_array_index(conn->destinations, j);
          
          ep->ref = g_hash_table_lookup(cfg->destinations, ep->name->str);
          if (!ep->ref)
            {
              msg_error("Error in configuration, unresolved destination reference",
                        evt_tag_str("filter", ep->name->str),
                        NULL);
              return FALSE;
            }
          log_dgrp_ref(ep->ref);
        }
    }

  self->state = LC_STATE_INIT_SOURCES;
  self->success = TRUE;  
  g_hash_table_foreach(cfg->sources, (GHFunc) log_center_init_component, self);
  if (!self->success)
    return FALSE;

  self->success = TRUE;    
  self->state = LC_STATE_INIT_DESTS;
  g_hash_table_foreach(cfg->destinations, (GHFunc) log_center_init_component, self);
  if (!self->success)
    return FALSE;
  
  self->state = LC_STATE_WORKING;
  return TRUE;
}

static gboolean
log_center_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogCenter *self = (LogCenter *) s;
  
  self->cfg = cfg;
  self->persist = persist;
  self->success = TRUE;
  g_hash_table_foreach(cfg->sources, (GHFunc) log_center_deinit_component, self);
  if (!self->success)
    return FALSE;
  
  self->success = TRUE;
  g_hash_table_foreach(cfg->destinations, (GHFunc) log_center_deinit_component, self);
  if (!self->success)
    return FALSE;
    
  return TRUE;
}

static void
log_center_ack(LogMessage *msg, gpointer user_data)
{
  log_msg_ack_block_end(msg);
  log_msg_ack(msg);
  log_msg_unref(msg);
}

static void
log_center_queue(LogPipe *s, LogMessage *msg, gint path_flags)
{
  LogCenter *self = (LogCenter *) s;
  gboolean match, fallbacks, have_fallbacks = 1;
  gint ci, fi, di;
  
  afinter_postpone_mark(self->cfg->mark_freq);

  log_msg_ref(msg);
  log_msg_ack_block_start(msg, log_center_ack, NULL);
  
  for (match = 0, fallbacks = 0; !match && have_fallbacks && (fallbacks <= 1); fallbacks++)
    {
      have_fallbacks = 0;
      
      for (ci = 0; ci < self->cfg->connections->len; ci++)
        {
          LogConnection *conn = (LogConnection *) g_ptr_array_index(self->cfg->connections, ci);
          
          if (!fallbacks && (conn->flags & LC_FALLBACK))
            {
              have_fallbacks = 1;
              continue;
            }
          else if (fallbacks && !(conn->flags & LC_FALLBACK))
            {
              continue;
            }
            
          if (!(conn->flags & LC_CATCHALL))
            {
              /* check source */
              if (!g_hash_table_lookup(conn->source_cache, msg->source_group->name->str))
                {
                  goto next_connection;
                }
            }
          else
            {
              /* catchall, every source matches */
              ;
            }
      
          for (fi = 0; fi < conn->filters->len; fi++)
            {
              LogEndpoint *ep = (LogEndpoint *) g_ptr_array_index(conn->filters, fi);
              LogFilterRule *f;
                  
              f = (LogFilterRule *) ep->ref;
              if (!log_filter_rule_eval(f, msg))
                {
                  goto next_connection;
                }
            }
          match = 1;
          
          for (di = 0; di < conn->destinations->len; di++)
            {
              LogEndpoint *ep = (LogEndpoint *) g_ptr_array_index(conn->destinations, di);
              LogDestGroup *dest;
              
              if (conn->flags & LC_FLOW_CONTROL)
                log_msg_ack_block_inc(msg);
              
              dest = (LogDestGroup *) ep->ref;
              log_pipe_queue(&dest->super, log_msg_ref(msg), path_flags | ((conn->flags & LC_FLOW_CONTROL) ? 0 : PF_FLOW_CTL_OFF));
            }
          
          if (conn->flags & LC_FINAL)
            {
              break;
            }
        next_connection:
          ;
        }
    }
  /* our own ack */
  log_msg_ack(msg);
}

static void
log_center_free(LogPipe *s)
{
  g_free(s);
}

LogCenter *
log_center_new()
{
  LogCenter *self = g_new0(LogCenter, 1);

  log_pipe_init_instance(&self->super);
  self->super.init = log_center_init;
  self->super.deinit = log_center_deinit;
  self->super.queue = log_center_queue;
  self->super.free_fn = log_center_free;
  return self;
}
