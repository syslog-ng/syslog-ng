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
  
#ifndef LOGPIPE_H_INCLUDED
#define LOGPIPE_H_INCLUDED

#include "syslog-ng.h"
#include "logmsg.h"
#include "cfg.h"
#include "atomic.h"

/* notify code values */
#define NC_CLOSE       1
#define NC_READ_ERROR  2
#define NC_WRITE_ERROR 3
#define NC_FILE_MOVED  4
#define NC_FILE_EOF    5
#define NC_FILE_SKIP   6

#define PIF_INITIALIZED  0x0001
#define PIF_FINAL        0x0002
#define PIF_FALLBACK     0x0004
#define PIF_FLOW_CONTROL 0x0008
#define PIF_CLONE        0x0010
#define PIF_INLINED      0x0020

/* some more flags are defined in logmpx.h */

/**
 *
 * Processing pipeline
 *
 *   Within syslog-ng, the user configuration is converted into a tree-like
 *   structure.  It's node in this tree is a LogPipe object responsible for
 *   queueing message towards the destination.  Each node is free to
 *   drop/transform the message it receives.
 *
 *   The center.c module contains code that transforms the configuration
 *   into the log processing tree.  Each log statement in user configuration
 *   becomes a linked list of pipes, then each source, referenced by the
 *   is piped into the newly created pipe.
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
 **/

struct _LogPathOptions
{
  gboolean flow_control:1;
  gboolean *matched;
};

#define LOG_PATH_OPTIONS_INIT { TRUE, NULL }

typedef struct _LogPipe
{
  GAtomicCounter ref_cnt;
  gint32 flags;
  GlobalConfig *cfg;
  struct _LogPipe *pipe_next;
  void (*queue)(struct _LogPipe *self, LogMessage *msg, const LogPathOptions *path_options);
  gboolean (*init)(struct _LogPipe *self);
  gboolean (*deinit)(struct _LogPipe *self);
  void (*free_fn)(struct _LogPipe *self);
  void (*notify)(struct _LogPipe *self, struct _LogPipe *sender, gint notify_code, gpointer user_data);
} LogPipe;


LogPipe *log_pipe_ref(LogPipe *self);
void log_pipe_unref(LogPipe *self);
void log_pipe_init_instance(LogPipe *self);
void log_pipe_forward_msg(LogPipe *self, LogMessage *msg, const LogPathOptions *path_options);
void log_pipe_forward_notify(LogPipe *self, LogPipe *sender, gint notify_code, gpointer user_data);


static inline GlobalConfig *
log_pipe_get_config(LogPipe *s)
{
  return s->cfg;
}

static inline gboolean
log_pipe_init(LogPipe *s, GlobalConfig *cfg)
{
  if (!(s->flags & PIF_INITIALIZED))
    {
      s->cfg = cfg;
      if (!s->init || s->init(s))
        {
          s->flags |= PIF_INITIALIZED;
          return TRUE;
        }
      return FALSE;
    }
  return TRUE;
}

static inline gboolean
log_pipe_deinit(LogPipe *s)
{
  if ((s->flags & PIF_INITIALIZED))
    {
      if (!s->deinit || s->deinit(s))
        {
          s->cfg = NULL;

          s->flags &= ~PIF_INITIALIZED;
          return TRUE;
        }
      s->cfg = NULL;
      return FALSE;
    }
  return TRUE;
}

static inline void
log_pipe_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  s->queue(s, msg, path_options);
}

static inline void
log_pipe_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  if (s->notify)
    s->notify(s, sender, notify_code, user_data);
}

static inline void
log_pipe_append(LogPipe *s, LogPipe *next)
{
  s->pipe_next = next;
}

void log_pipe_free_method(LogPipe *s);

#endif
