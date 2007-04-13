#ifndef LOGPIPE_H_INCLUDED
#define LOGPIPE_H_INCLUDED

#include "logmsg.h"
#include "cfg.h"

#include <glib.h>

/* notify code values */
#define NC_CLOSE       1
#define NC_READ_ERROR  2
#define NC_WRITE_ERROR 3

#define PF_FLOW_CTL_OFF 0x0001

typedef struct _LogPipe
{
  guint ref_cnt;
  struct _LogPipe *pipe_next;
  gboolean (*init)(struct _LogPipe *self, GlobalConfig *cfg, PersistentConfig *persist);
  gboolean (*deinit)(struct _LogPipe *self, GlobalConfig *cfg, PersistentConfig *persist);
  void (*queue)(struct _LogPipe *self, LogMessage *msg, gint path_flags);
  void (*free_fn)(struct _LogPipe *self);
  void (*notify)(struct _LogPipe *self, struct _LogPipe *sender, gint notify_code, gpointer user_data);
} LogPipe;


LogPipe *log_pipe_ref(LogPipe *self);
void log_pipe_unref(LogPipe *self);
void log_pipe_init_instance(LogPipe *self);
void log_pipe_forward_msg(LogPipe *self, LogMessage *msg, gint path_flags);
void log_pipe_forward_notify(LogPipe *self, LogPipe *sender, gint notify_code, gpointer user_data);

static inline gboolean
log_pipe_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  return s->init(s, cfg, persist);
}

static inline gboolean
log_pipe_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  return s->deinit(s, cfg, persist);
}

static inline void
log_pipe_queue(LogPipe *s, LogMessage *msg, gint path_flags)
{
  s->queue(s, msg, path_flags);
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

#endif
