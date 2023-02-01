/*
 * Copyright (c) 2002-2013, 2015 Balabit
 * Copyright (c) 1998-2013, 2015 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#ifndef PATTERNDB_CORRELATION_CONTEXT_H_INCLUDED
#define PATTERNDB_CORRELATION_CONTEXT_H_INCLUDED

#include "syslog-ng.h"
#include "correlation-key.h"
#include "timerwheel.h"
#include "logmsg/logmsg.h"
#include "template/templates.h"

/* This class encapsulates a correlation context, keyed by CorrelationKey, type == PSK_RULE. */
typedef struct _CorrelationContext CorrelationContext;
struct _CorrelationContext
{
  /* key in the hashtable. */
  CorrelationKey key;
  /* timeout timer */
  TWEntry *timer;
  /* messages belonging to this context */
  GPtrArray *messages;
  gint ref_cnt;
  void (*free_fn)(CorrelationContext *s);
};

static inline LogMessage *
correlation_context_get_last_message(CorrelationContext *self)
{
  return (LogMessage *) g_ptr_array_index(self->messages, self->messages->len - 1);
}

void correlation_context_init(CorrelationContext *self, const CorrelationKey *key);
void correlation_context_free_method(CorrelationContext *self);
void correlation_context_sort(CorrelationContext *self, LogTemplate *sort_key);
CorrelationContext *correlation_context_new(CorrelationKey *key);
CorrelationContext *correlation_context_ref(CorrelationContext *self);
void correlation_context_unref(CorrelationContext *self);


#endif
