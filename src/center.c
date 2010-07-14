/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
  
#include "center.h"
#include "sgroup.h"
#include "dgroup.h"
#include "filter.h"
#include "messages.h"
#include "afinter.h"
#include "stats.h"
#include "logparser.h"
#include "logmpx.h"

#include <string.h>

/**
 *
 * Processing pipeline
 *
 *   This module builds a pipe-line of LogPipe objects, described by a log
 *   statement in the user configuration. Then each source, referenced by the
 *   LogConnection is piped into the newly created pipe.
 *  
 *   Something like this:
 *
 *    log statement:
 *       mpx | filter | parser | dest1 | dest2 | dest3
 *
 *    source1 -> log statement1
 *            |-> log statement2
 *
 *   E.g. each source is sending to each log path it was referenced from. Each
 *   item in the log path is a pipe, which receives messages and forwards it
 *   at its discretion. Filters are pipes too, which lose data. Destinations
 *   are piping their output to the next element on the pipeline. This
 *   basically means that the pipeline is a wired representation of the user
 *   configuration without having to loop through configuration data.
 *
 * Reference counting
 *
 *   The pipes do not reference each other through their pipe_next member,
 *   simply because there'd be too much reference loops to care about. 
 *   Instead pipe_next is a borrowed reference, which is assumed to be valid
 *   as long as the configuration is not freed.
 *
 *   As there are pipes that are dynamically created during init, these
 *   references must be dropped. The LogCenter->initialized_pipes array has
 *   the following use:
 *
 *      - it is used to init/deinit pipes in the complete configuration 
 *      - it is used to _the_ reference from config->pipe which makes the
 *        assumption above true (e.g. that pipe_next is valid as long as the
 *        configuration is not freed)
 *
 **/


struct _LogPipeItem
{
  struct _LogPipeItem *ep_next;
  GString *name;
  gint type;
  gpointer ref;
};


/**
 * log_pipe_item_append:
 * @a: first LogPipeItem
 * @b: second LogPipeItem
 *
 * This function appends @b to @a in a linked list using the ep_next field
 * in LogPipeItem.
 **/
void
log_pipe_item_append(LogPipeItem *a, LogPipeItem *b)
{
  a->ep_next = b;
}

LogPipeItem *
log_pipe_item_append_tail(LogPipeItem *a, LogPipeItem *b)
{
  if (a)
    {
      LogPipeItem *p = a;
      while (p->ep_next)
        p = p->ep_next;
      log_pipe_item_append(p, b);
      return a;
    }
  return b;
}

/**
 * log_pipe_item_new:
 * @type: specifies endpoint type one of EP_* 
 * @name: name of the endpoint which is used in the configuration as an identifier
 * 
 * This function constructs a LogPipeItem object encapsulating a
 * source/filter/destination node in a log statement.
 **/
LogPipeItem *
log_pipe_item_new(gint type, gchar *name)
{
  LogPipeItem *self = g_new0(LogPipeItem, 1);
  
  self->type = type;
  self->name = g_string_new(name);
  return self;
}

/**
 * log_pipe_item_new_ref:
 * @type: specifies endpoint type one of EP_* 
 * @ref: reference to the endpoint (used for unnamed PIPE items)
 * 
 * This function constructs a LogPipeItem object encapsulating a
 * source/filter/destination node in a log statement.
 **/
LogPipeItem *
log_pipe_item_new_ref(gint type, gpointer ref)
{
  LogPipeItem *self = g_new0(LogPipeItem, 1);
  
  self->type = type;
  self->name = NULL;
  self->ref = ref;
  return self;
}

/**
 * log_pipe_item_free:
 * @self: LogPipeItem instance
 * 
 * This function frees a LogPipeItem instance, first by freeing the
 * referenced object stored in the @ref attribute then freeing @self.
 * NOTE: @ref is set when the given configuration is initialized.
 **/
void
log_pipe_item_free(LogPipeItem *self)
{
  if (self->name)
    g_string_free(self->name, TRUE);
  if (self->ref)
    {
      switch (self->type)
        {
        case EP_SOURCE:
          log_source_group_unref((LogSourceGroup *) self->ref);
          break;
        case EP_FILTER:
          log_process_rule_unref((LogProcessRule *) self->ref);
          break;
        case EP_PARSER:
          log_process_rule_unref((LogProcessRule *) self->ref);
          break;
        case EP_DESTINATION:
          log_dest_group_unref((LogDestGroup *) self->ref);
          break;
        case EP_PIPE:
          log_connection_free((LogConnection *) self->ref);
          break;
        case EP_REWRITE:
          log_process_rule_unref((LogProcessRule *) self->ref);
          break;
        default:
          g_assert_not_reached();
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
 * and destinations each represented by a LogPipeItem object.
 **/
LogConnection *
log_connection_new(LogPipeItem *endpoints, guint32 flags)
{
  LogConnection *self = g_new0(LogConnection, 1);
  
  self->flags = flags;
  self->endpoints = endpoints;
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
  LogPipeItem *ep, *ep_next;
  
  for (ep = self->endpoints; ep; ep = ep_next)
    {
      ep_next = ep->ep_next;
      log_pipe_item_free(ep);
    }
  g_free(self);
}

gint
log_connection_lookup_flag(const gchar *flag)
{
  if (strcmp(flag, "catch-all") == 0)
    return LC_CATCHALL;
  else if (strcmp(flag, "fallback") == 0)
    return LC_FALLBACK;
  else if (strcmp(flag, "final") == 0)
    return LC_FINAL;
  else if (strcmp(flag, "flow_control") == 0 || strcmp(flag, "flow-control") == 0)
    return LC_FLOW_CONTROL;
  msg_error("Unknown log statement flag", evt_tag_str("flag", flag), NULL);
  return 0;
}

static void
log_center_connect_source(gpointer key, gpointer value, gpointer user_data)
{
  LogPipe *pipe = (LogPipe *) value;
  LogCenter *self = ((gpointer *) user_data)[0];
  LogPipe *first_pipe = ((gpointer *) user_data)[1];
  LogMultiplexer *mpx;

  if (!pipe->pipe_next)
    {
      mpx = log_multiplexer_new(PIF_MPX_FLOW_CTRL_BARRIER);
      /* NOTE: initialized_pipes holds a ref */
      g_ptr_array_add(self->initialized_pipes, mpx);
      log_pipe_append(pipe, &mpx->super);
    }
  
  mpx = (LogMultiplexer *) pipe->pipe_next;
  log_multiplexer_add_next_hop(mpx, first_pipe);
  g_ptr_array_add(self->initialized_pipes, log_pipe_ref(pipe));
}

/* NOTE: returns a borrowed reference! */
LogPipe *
log_center_init_pipe_line(LogCenter *self, LogConnection *conn, GlobalConfig *cfg, gboolean toplevel, gboolean flow_controlled_parent)
{
  LogPipeItem *ep;
  LogPipe *first_pipe, *pipe, *last_pipe, *sub_pipe;
  LogMultiplexer *mpx, *fork_mpx = NULL;
  gboolean path_changes_the_message = FALSE, flow_controlled_child = FALSE;

  /* resolve pipe references, find first pipe */
  
  if (!toplevel && (conn->flags & LC_CATCHALL) != 0)
    {
      msg_error("Error in configuration, catch-all flag can only be specified for top-level log statements",
                NULL);
      goto error;
    }
  
  first_pipe = last_pipe = NULL;
  
  pipe = NULL;
  for (ep = conn->endpoints; ep; ep = ep->ep_next)
    {
      g_assert(pipe == NULL);

      /* this switch results in a borrowed reference to be stored in @pipe */
      switch (ep->type)
        {
        case EP_SOURCE:
          if (toplevel && (conn->flags & LC_CATCHALL) == 0)
            {
              ep->ref = g_hash_table_lookup(cfg->sources, ep->name->str);
              if (!ep->ref)
                {
                  msg_error("Error in configuration, unresolved source reference",
                            evt_tag_str("source", ep->name->str),
                            NULL);
                  goto error;
                }
              log_source_group_ref((LogSourceGroup *) ep->ref);
              pipe = (LogPipe *) ep->ref;
              g_ptr_array_add(self->initialized_pipes, log_pipe_ref(pipe));

              if (!pipe->pipe_next)
                {
                  mpx = log_multiplexer_new(PIF_MPX_FLOW_CTRL_BARRIER);
                  g_ptr_array_add(self->initialized_pipes, mpx);
                  log_pipe_append(pipe, &mpx->super);
                }
              pipe = NULL;
            }
          else if (!toplevel)
            {
              msg_error("Error in configuration, no source reference is permitted in non-toplevel log statements",
                        NULL);
              goto error;
            }
          else
            {
              msg_error("Error in configuration, catch-all log statements may not specify sources",
                        NULL);
              goto error;
            }
          break;
        case EP_FILTER:
          {
            LogProcessRule *rule;

            rule = ep->ref = g_hash_table_lookup(cfg->filters, ep->name->str);
            if (!ep->ref)
              {
                msg_error("Error in configuration, unresolved filter reference",
                          evt_tag_str("filter", ep->name->str),
                          NULL);
                goto error;
              }
            log_process_rule_ref(rule);
            pipe = log_process_pipe_new(rule);
            g_ptr_array_add(self->initialized_pipes, pipe);
            if (rule->modify)
              path_changes_the_message = TRUE;
            break;
          }
        case EP_PARSER:
          ep->ref = g_hash_table_lookup(cfg->parsers, ep->name->str);
          if (!ep->ref)
            {
              msg_error("Error in configuration, unresolved parser reference",
                        evt_tag_str("parser", ep->name->str),
                        NULL);
              goto error;
            }
          log_process_rule_ref(ep->ref);
          pipe = log_process_pipe_new((LogProcessRule *) ep->ref);
          g_ptr_array_add(self->initialized_pipes, pipe);
          path_changes_the_message = TRUE;
          break;
        case EP_DESTINATION:
          ep->ref = g_hash_table_lookup(cfg->destinations, ep->name->str);
          if (!ep->ref)
            {
              msg_error("Error in configuration, unresolved destination reference",
                        evt_tag_str("destination", ep->name->str),
                        NULL);
              return FALSE;
            }
          log_dest_group_ref((LogDestGroup *) ep->ref);
          g_ptr_array_add(self->initialized_pipes, log_pipe_ref((LogPipe *) ep->ref));
          
          pipe = (LogPipe *) log_multiplexer_new(0);
          log_multiplexer_add_next_hop((LogMultiplexer *) pipe, ep->ref);
          g_ptr_array_add(self->initialized_pipes, pipe);
          break;
        case EP_PIPE:
        
          if (!fork_mpx)
            {
              fork_mpx = log_multiplexer_new(PIF_MPX_FLOW_CTRL_BARRIER);
              pipe = &fork_mpx->super;
              g_ptr_array_add(self->initialized_pipes, pipe);
            }
          sub_pipe = log_center_init_pipe_line(self, (LogConnection *) ep->ref, cfg, FALSE, (conn->flags & LC_FLOW_CONTROL));
          if (!sub_pipe)
            {
              /* error initializing subpipe */
              goto error;
            }
          log_multiplexer_add_next_hop(fork_mpx, sub_pipe);
          if (sub_pipe->flags & PIF_FLOW_CONTROL)
            flow_controlled_child = TRUE;
          break;
        case EP_REWRITE:
          ep->ref = g_hash_table_lookup(cfg->rewriters, ep->name->str);
          if (!ep->ref)
            {
              msg_error("Error in configuration, unresolved rewrite reference",
                        evt_tag_str("rewrite", ep->name->str),
                        NULL);
              goto error;
            }
          log_process_rule_ref(ep->ref);
          pipe = log_process_pipe_new((LogProcessRule *) ep->ref);
          g_ptr_array_add(self->initialized_pipes, pipe);
          path_changes_the_message = TRUE;
          break;
        default:
          g_assert_not_reached();
          break;
        }
        
      /* pipe is only a borrowed reference */
      if (pipe)
        {
          if (!first_pipe)
            first_pipe = pipe;

          if (last_pipe)
            {
              log_pipe_append(last_pipe, pipe);
              last_pipe = pipe;
              pipe = NULL;
            }
          else
            {
              last_pipe = pipe;
              pipe = NULL;
            }

          while (last_pipe->pipe_next)
            last_pipe = last_pipe->pipe_next;
        }
    }
  
  if (!first_pipe)
    {
      /* FIXME: more accurate description of the error */
      msg_error("Pipeline has no processing elements, only sources", NULL);
      goto error;
    }
  
  if (conn->flags & LC_FALLBACK)
    first_pipe->flags |= PIF_FALLBACK;
  if (conn->flags & LC_FINAL)
    first_pipe->flags |= PIF_FINAL;
  if ((conn->flags & LC_FLOW_CONTROL) || flow_controlled_child || flow_controlled_parent)
    first_pipe->flags |= PIF_FLOW_CONTROL;
  if (path_changes_the_message)
    first_pipe->flags |= PIF_CLONE;
    
  if ((conn->flags & LC_CATCHALL) == 0)
    {
      for (ep = conn->endpoints; ep; ep = ep->ep_next)
        {
          if (ep->type == EP_SOURCE)
            {
              pipe = (LogPipe *) ep->ref;
              mpx = (LogMultiplexer *) pipe->pipe_next;
              log_multiplexer_add_next_hop(mpx, first_pipe);
              pipe = NULL;
            }
        }
    }
  else
    {
      gpointer args[] = { self, first_pipe };
      g_hash_table_foreach(cfg->sources, log_center_connect_source, args);
    }
  return first_pipe;
 error:
  
  /* we don't need to free anything, everything we allocated is recorded in
   * @self, thus will be freed whenever log_center_free is called */
  
  return NULL;
}

gboolean
log_center_init(LogCenter *self, GlobalConfig *cfg)
{
  gint i;
  
  /* resolve references within the configuration */
  
  for (i = 0; i < cfg->connections->len; i++)
    {
      LogConnection *conn = (LogConnection *) g_ptr_array_index(cfg->connections, i);
      LogPipe *pipe_line;
      
      pipe_line = log_center_init_pipe_line(self, conn, cfg, TRUE, FALSE);
      if (!pipe_line)
        {
          return FALSE;
        }
    }

  for (i = 0; i < self->initialized_pipes->len; i++)
    {
      if (!log_pipe_init(g_ptr_array_index(self->initialized_pipes, i), cfg))
        {
          msg_error("Error initializing message pipeline",
                    NULL);
          return FALSE;
        }
    }
  
  stats_register_counter(0, SCS_CENTER, NULL, "received", SC_TYPE_PROCESSED, &self->received_messages);
  stats_register_counter(0, SCS_CENTER, NULL, "queued", SC_TYPE_PROCESSED, &self->queued_messages);
  
  self->state = LC_STATE_WORKING;
  return TRUE;
}

gboolean
log_center_deinit(LogCenter *self)
{
  gboolean success = TRUE;
  gint i;

  for (i = 0; i < self->initialized_pipes->len; i++)
    {
      if (!log_pipe_deinit(g_ptr_array_index(self->initialized_pipes, i)))
        success = FALSE;
    }
  
  stats_unregister_counter(SCS_CENTER, NULL, "received", SC_TYPE_PROCESSED, &self->received_messages);
  stats_unregister_counter(SCS_CENTER, NULL, "queued", SC_TYPE_PROCESSED, &self->queued_messages);
  return success;
}

void
log_center_free(LogCenter *self)
{
  gint i;
  for (i = 0; i < self->initialized_pipes->len; i++)
    {
      log_pipe_unref(g_ptr_array_index(self->initialized_pipes, i));
    }
  g_ptr_array_free(self->initialized_pipes, TRUE);
  g_free(self);
}

LogCenter *
log_center_new()
{
  LogCenter *self = g_new0(LogCenter, 1);

  self->initialized_pipes = g_ptr_array_new();
  return self;
}
