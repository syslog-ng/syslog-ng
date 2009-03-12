/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
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

#ifndef LOGPROCESS_H_INCLUDED
#define LOGPROCESS_H_INCLUDED

#include "logmsg.h"
#include "logpipe.h"


/**
 * This class encapsulates a processing rule (currently filter or parser) as
 * present in the configuration. It defines a common interface that can be
 * implemented by various processing elements, e.g. it can be extended for
 * rewrite rules
 **/
typedef struct _LogProcessRule LogProcessRule;
struct _LogProcessRule
{
  gint ref_cnt;
  gchar *name;
  /* this processing element changes the logmessage */
  gboolean modify;
  gboolean (*process)(LogProcessRule *s, LogMessage *msg);
  void (*free_fn)(LogProcessRule *s);
};

static inline gboolean
log_process_rule_process(LogProcessRule *s, LogMessage *msg)
{
  return s->process(s, msg);
}

void log_process_rule_init(LogProcessRule *self, const gchar *name);
void log_process_rule_ref(LogProcessRule *self);
void log_process_rule_unref(LogProcessRule *self);


/**
 * This class encapsulates a LogPipe that performs some kind of processing.
 * It uses the LogProcessRule object to perform its work, but the same
 * LogProcessPipe object can be reused for different LogProcessRules. This
 * means that filter/parser/rewrite all use this pipe.
 **/
typedef struct _LogProcessPipe LogProcessPipe;
struct _LogProcessPipe
{
  LogPipe super;
  LogProcessRule *rule;
};

LogPipe *log_process_pipe_new(LogProcessRule *rule);


#endif
