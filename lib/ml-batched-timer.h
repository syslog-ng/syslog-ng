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

#ifndef ML_BATCHED_TIMER_INCLUDED
#define ML_BATCHED_TIMER_INCLUDED

#include "mainloop.h"
#include <iv.h>


/* timer which only updates */
typedef struct _MlBatchedTimer
{
  GMutex lock;
  struct iv_timer timer;
  struct timespec expires;
  gpointer cookie;
  void *(*ref_cookie)(gpointer self);
  void (*unref_cookie)(gpointer self);
  void (*handler)(gpointer self);
} MlBatchedTimer;

void ml_batched_timer_postpone(MlBatchedTimer *self, glong sec);
void ml_batched_timer_cancel(MlBatchedTimer *self);

void ml_batched_timer_unregister(MlBatchedTimer *self);
void ml_batched_timer_init(MlBatchedTimer *self);
void ml_batched_timer_free(MlBatchedTimer *self);


#endif
