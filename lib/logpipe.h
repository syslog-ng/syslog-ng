/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include "logmsg/logmsg.h"
#include "cfg.h"
#include "atomic.h"
#include "messages.h"

/* notify code values */
#define NC_CLOSE       1
#define NC_READ_ERROR  2
#define NC_WRITE_ERROR 3
#define NC_FILE_MOVED  4
#define NC_FILE_EOF    5
#define NC_REOPEN_REQUIRED 6
#define NC_FILE_DELETED 7

/* indicates that the LogPipe was initialized */
#define PIF_INITIALIZED       0x0001
/* indicates that this LogPipe got cloned into the tree already */
#define PIF_INLINED           0x0002

/* log statement flags that are copied to the head of a branch */
#define PIF_BRANCH_FINAL      0x0004
#define PIF_BRANCH_FALLBACK   0x0008
#define PIF_BRANCH_PROPERTIES (PIF_BRANCH_FINAL + PIF_BRANCH_FALLBACK)

#define PIF_DROP_UNMATCHED    0x0010

/* branch starting with this pipe wants hard flow control */
#define PIF_HARD_FLOW_CONTROL 0x0020

/* this pipe is a source for messages, it is not meant to be used to
 * forward messages, syslog-ng will only use these pipes for the
 * left-hand side of the processing graph, e.g. no other pipes may be
 * sending messages to these pipes and these are expected to generate
 * messages "automatically". */

#define PIF_SOURCE            0x0040

/* private flags range, to be used by other LogPipe instances for their own purposes */

#define PIF_PRIVATE(x)       ((x) << 16)

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
 * Flow control
 *
 *   Flow control is the mechanism used to control the message rate between
 *   input/output sides of syslog-ng in order to avoid message loss.  If the
 *   two sides were independent, the input side could well receive messages
 *   at a much higher rate than the destination is able to cope with.
 *
 *   This is implemented by allocating a per-source window (similar to a TCP
 *   window), which can be "filled" by the source without the danger of
 *   output queue overflow.  Also, whenever a message is processed by the
 *   destination it invokes an ACK, which in turn increments the window size.
 *
 *   This basically boils down to the following:
 *     * the source is free to receive as much messages as fits into its window
 *     * whenever the destination has processed a message, this is signalled
 *       to freeing up a lot in its window
 *     * if the message is full, the source is suspended, no further messages
 *       are received.
 *
 *   This controls the message rate but doesn't completely ruin throughput,
 *   as the source has some space without being suspended, as suspension and
 *   resuming action takes considerable amount of time (mostly latency, but
 *   CPU is certainly also used).
 *
 *   There are currently two forms of flow control:
 *     * hard flow control
 *     * soft flow control
 *
 *   The first is the form of flow control present in earlier syslog-ng
 *   versions and was renamed as "hard" in order to differentiate from the
 *   other form.  Hard means that the source is completely suspended until
 *   the destination indeed processed a message.  If the network is down,
 *   the disk is full, the source will not accept messages.
 *
 *   Soft flow control was introduced when syslog-ng became threaded and the
 *   earlier priority based behaviour couldn't be mimiced any other way.
 *   Soft flow control cannot be configured, it is automatically used by
 *   file destinations if "hard" flow control is not enabled by the user.
 *   Soft flow control means that flow is only controlled as long as the
 *   destination is writable, if an error occurs (disk full, etc) messages
 *   get dropped on the floor.  But as long as the destination is writable,
 *   the destination rate controls the source rate as well.
 *
 *   The behaviour in non-threaded syslog-ng was, that destinations were
 *   prioritized over sources, and whenever a destination was writable,
 *   sources were implicitly suspended.  This is not easily implementable by
 *   threads and ivykis, thus this alternative mechanism was created.
 *
 *   Please note that soft-flow-control is a somewhat stronger guarantee
 *   than the earlier behaviour, therefore it is currently only used for
 *   destination files.
 *
 * Plugin overrides
 *
 *   Various methods can be overridden by external objects within
 *   LogPipe and derived classes. The aim of this functionality to
 *   make it possible to attach new functions to a LogPipe at runtime.
 *
 *   For example, it'd make sense to implement the "suppress"
 *   functionality as such plugin, which is currently implemented in
 *   LogWriter, and in case a non-LogWriter destination would need it,
 *   then a separate implementation would be needed.
 *
 *   The way to override a method by an external object is as follows:
 *
 *     - it should save the current value of the method address (for
 *       example "queue" for the queue method)
 *
 *     - it should change the pointer pointing to the relevant method to
 *       its own code (e.g. change "queue" in LogPipe)
 *
 *     - once the hook is invoked, it should take care about calling the
 *       original function
 **/

struct _LogPathOptions
{
  /* an acknowledgement is "passed" to this path, an ACK is still
   * needed to close the window slot. This was called "flow-control"
   * and meant both of these things: the user requested
   * flags(flow-control), _AND_ an acknowledgement was needed. With
   * the latest change, the one below specifies the user option,
   * while the "ack is still needed" condition is stored in
   * ack_needed.
   */

  gboolean ack_needed;

  /* The user has requested flow-control on this processing path,
   * which means that the destination should invoke log_msg_ack()
   * after it has completed processing it (e.g. after sending to the
   * actual destination, possibly after confirmation if the transport
   * supports that). If flow-control is not requested, destinations
   * are permitted to call log_msg_ack() early (e.g. at queue time).
   *
   * This is initially FALSE and can be set to TRUE anywhere _before_
   * the destination driver, which will actually carry out the
   * required action.
   */

  gboolean flow_control_requested;

  gboolean *matched;
};

#define LOG_PATH_OPTIONS_INIT { TRUE, FALSE, NULL }
#define LOG_PATH_OPTIONS_INIT_NOACK { FALSE, FALSE, NULL }

struct _LogPipe
{
  GAtomicCounter ref_cnt;
  gint32 flags;

  void (*queue)(LogPipe *self, LogMessage *msg, const LogPathOptions *path_options);

  GlobalConfig *cfg;
  LogExprNode *expr_node;
  LogPipe *pipe_next;
  StatsCounterItem *discarded_messages;
  const gchar *persist_name;
  gchar *plugin_name;

  gboolean (*pre_init)(LogPipe *self);
  gboolean (*init)(LogPipe *self);
  gboolean (*deinit)(LogPipe *self);
  void (*post_deinit)(LogPipe *self);

  const gchar *(*generate_persist_name)(const LogPipe *self);

  /* clone this pipe when used in multiple locations in the processing
   * pipe-line. If it contains state, it should behave as if it was
   * the same instance, otherwise it can be a copy.
   */
  LogPipe *(*clone)(LogPipe *self);

  void (*free_fn)(LogPipe *self);
  void (*notify)(LogPipe *self, gint notify_code, gpointer user_data);
};

/*
   cpu-cache optimization: queue method should be as close as possible to flags.

   Rationale: the pointer pointing to this LogPipe instance is
   resolved right before calling queue and the colocation of flags and
   the queue pointer ensures that they are on the same cache line. The
   queue method is probably the single most often called virtual method
 */
G_STATIC_ASSERT(G_STRUCT_OFFSET(LogPipe, queue) - G_STRUCT_OFFSET(LogPipe, flags) <= 4);

extern gboolean (*pipe_single_step_hook)(LogPipe *pipe, LogMessage *msg, const LogPathOptions *path_options);

LogPipe *log_pipe_ref(LogPipe *self);
void log_pipe_unref(LogPipe *self);
LogPipe *log_pipe_new(GlobalConfig *cfg);
void log_pipe_init_instance(LogPipe *self, GlobalConfig *cfg);
void log_pipe_forward_notify(LogPipe *self, gint notify_code, gpointer user_data);
EVTTAG *log_pipe_location_tag(LogPipe *pipe);
void log_pipe_attach_expr_node(LogPipe *self, LogExprNode *expr_node);
void log_pipe_detach_expr_node(LogPipe *self);

static inline GlobalConfig *
log_pipe_get_config(LogPipe *s)
{
  return s->cfg;
}

static inline void
log_pipe_set_config(LogPipe *s, GlobalConfig *cfg)
{
  s->cfg = cfg;
}

static inline void
log_pipe_reset_config(LogPipe *s)
{
  log_pipe_set_config(s, NULL);
}

static inline gboolean
log_pipe_init(LogPipe *s)
{
  if (!(s->flags & PIF_INITIALIZED))
    {
      if (s->pre_init && !s->pre_init(s))
        return FALSE;
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
          s->flags &= ~PIF_INITIALIZED;

          if (s->post_deinit)
            s->post_deinit(s);
          return TRUE;
        }
      return FALSE;
    }
  return TRUE;
}

static inline void
log_pipe_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options);

static inline void
log_pipe_forward_msg(LogPipe *self, LogMessage *msg, const LogPathOptions *path_options)
{
  if (self->pipe_next)
    {
      log_pipe_queue(self->pipe_next, msg, path_options);
    }
  else
    {
      log_msg_drop(msg, path_options, AT_PROCESSED);
    }
}

static inline void
log_pipe_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogPathOptions local_path_options;
  g_assert((s->flags & PIF_INITIALIZED) != 0);

  if (G_UNLIKELY(pipe_single_step_hook))
    {
      if (!pipe_single_step_hook(s, msg, path_options))
        {
          log_msg_drop(msg, path_options, AT_PROCESSED);
          return;
        }
    }

  if (G_UNLIKELY(s->flags & (PIF_HARD_FLOW_CONTROL)))
    {
      local_path_options = *path_options;

      local_path_options.flow_control_requested = 1;
      path_options = &local_path_options;

      msg_trace("Requesting flow control", log_pipe_location_tag(s));
    }

  if (s->queue)
    {
      s->queue(s, msg, path_options);
    }
  else
    {
      log_pipe_forward_msg(s, msg, path_options);
    }

  if (path_options->matched && !(*path_options->matched) && (s->flags & PIF_DROP_UNMATCHED))
    {
      (*path_options->matched) = TRUE;
    }
}

static inline LogPipe *
log_pipe_clone(LogPipe *self)
{
  g_assert(NULL != self->clone);
  return self->clone(self);
}

static inline void
log_pipe_notify(LogPipe *s, gint notify_code, gpointer user_data)
{
  if (s->notify)
    s->notify(s, notify_code, user_data);
}

static inline void
log_pipe_append(LogPipe *s, LogPipe *next)
{
  s->pipe_next = next;
}

void
log_pipe_set_persist_name(LogPipe *self, const gchar *persist_name);

const gchar *
log_pipe_get_persist_name(const LogPipe *self);

void log_pipe_free_method(LogPipe *s);

#endif
