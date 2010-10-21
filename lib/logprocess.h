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

#ifndef LOGPROCESS_H_INCLUDED
#define LOGPROCESS_H_INCLUDED

#include "logmsg.h"
#include "logpipe.h"

/**
 * This class encapsulates a processing pipe, e.g. that makes some kind of
 * transformation on the message going through.  Such a pipe element may be
 * a filter, a parser or a rewrite rule.
 *
 * Also note, that the user configuration is parsed into a GList of
 * LogProcessPipes, which is then instantiated to all references, e.g.  if
 * the user has a single filter referenced from 3 locations, it'll be cloned
 * 2 times and inserted into the log processing pipeline.
 **/
typedef struct _LogProcessPipe LogProcessPipe;
struct _LogProcessPipe
{
  LogPipe super;
  /* clone this pipe for inlining */
  LogPipe *(*clone)(LogProcessPipe *self);
};

void log_process_pipe_init_instance(LogProcessPipe *self);
void log_process_pipe_free_method(LogPipe *s);

static inline LogPipe *
log_process_pipe_clone(LogProcessPipe *self)
{
  if (self->clone)
    return self->clone(self);
  return NULL;
}

/**
 * This class encapsulates a processing rule (currently
 * filter/parser/rewrite) as present in the configuration.  It contains a
 * list of LogPipe instances that can be inserted into the log processing
 * pipeline when the given filter/parser/rewrite rule is referenced.
 **/
typedef struct _LogProcessRule LogProcessRule;
struct _LogProcessRule
{
  gint ref_cnt;
  gchar *name;
  GList *head;
};

LogProcessRule *log_process_rule_new(const gchar *name, GList *head);
void log_process_rule_ref(LogProcessRule *self);
void log_process_rule_unref(LogProcessRule *self);

#endif
