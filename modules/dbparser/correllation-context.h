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
#ifndef PATTERNDB_CORRELLATION_CONTEXT_H_INCLUDED
#define PATTERNDB_CORRELLATION_CONTEXT_H_INCLUDED

#include "syslog-ng.h"
#include "correllation-key.h"
#include "timerwheel.h"
#include "logmsg/logmsg.h"
#include "template/templates.h"

/* This class encapsulates a correllation context, keyed by CorrellationKey, type == PSK_RULE. */
typedef struct _CorrellationContext CorrellationContext;
struct _CorrellationContext
{
  /* key in the hashtable. */
  CorrellationKey key;
  /* timeout timer */
  TWEntry *timer;
  /* messages belonging to this context */
  GPtrArray *messages;
  gint ref_cnt;
  void (*free_fn)(CorrellationContext *s);
};

static inline LogMessage *
correllation_context_get_last_message(CorrellationContext *self)
{
  return (LogMessage *) g_ptr_array_index(self->messages, self->messages->len - 1);
}

void correllation_context_init(CorrellationContext *self, const CorrellationKey *key);
void correllation_context_free_method(CorrellationContext *self);
void correllation_context_sort(CorrellationContext *self, LogTemplate *sort_key);
CorrellationContext *correllation_context_new(CorrellationKey *key);
CorrellationContext *correllation_context_ref(CorrellationContext *self);
void correllation_context_unref(CorrellationContext *self);


#endif
