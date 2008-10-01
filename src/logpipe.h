/*
 * Copyright (c) 2002-2008 BalaBit IT Ltd, Budapest, Hungary
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

/* some more flags are defined in logmpx.h */

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
      if (s->init(s))
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
      if (s->deinit(s))
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

void log_pipe_free(LogPipe *s);

#endif
