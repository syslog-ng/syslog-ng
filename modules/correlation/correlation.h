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
#ifndef CORRELATION_CORRELATION_H_INCLUDED
#define CORRELATION_CORRELATION_H_INCLUDED

#include "syslog-ng.h"
#include "correlation-key.h"
#include "correlation-context.h"
#include "timerwheel.h"
#include "timeutils/unixtime.h"

typedef struct _CorrelationState
{
  GAtomicCounter ref_cnt;
  GMutex lock;
  GHashTable *state;
  TimerWheel *timer_wheel;
  TWCallbackFunc expire_callback;
  struct timespec last_tick;
} CorrelationState;

void correlation_state_tx_begin(CorrelationState *self);
void correlation_state_tx_end(CorrelationState *self);
CorrelationContext *correlation_state_tx_lookup_context(CorrelationState *self, const CorrelationKey *key);
void correlation_state_tx_store_context(CorrelationState *self, CorrelationContext *context, gint timeout);
void correlation_state_tx_remove_context(CorrelationState *self, CorrelationContext *context);
void correlation_state_tx_update_context(CorrelationState *self, CorrelationContext *context, gint timeout);

void correlation_state_set_time(CorrelationState *self, guint64 sec, gpointer caller_context);
guint64 correlation_state_get_time(CorrelationState *self);
gboolean correlation_state_timer_tick(CorrelationState *self, gpointer caller_context);
void correlation_state_expire_all(CorrelationState *self, gpointer caller_context);
void correlation_state_advance_time(CorrelationState *self, gint timeout, gpointer caller_context);

void correlation_state_init_instance(CorrelationState *self);
void correlation_state_deinit_instance(CorrelationState *self);
CorrelationState *correlation_state_new(TWCallbackFunc expire);
CorrelationState *correlation_state_ref(CorrelationState *self);
void correlation_state_unref(CorrelationState *self);

#endif
