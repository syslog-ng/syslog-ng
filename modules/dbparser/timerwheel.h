/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#ifndef TIMERWHEEL_H_INCLUDED
#define TIMERWHEEL_H_INCLUDED

#include "syslog-ng.h"

typedef struct _TWEntry TWEntry;
typedef struct _TimerWheel TimerWheel;
typedef void (*TWCallbackFunc)(TimerWheel *tw, guint64 now, gpointer user_data);

TWEntry *timer_wheel_add_timer(TimerWheel *self, gint timeout, TWCallbackFunc cb, gpointer user_data,
                               GDestroyNotify user_data_free);
void timer_wheel_del_timer(TimerWheel *self, TWEntry *entry);
void timer_wheel_mod_timer(TimerWheel *self, TWEntry *entry, gint new_timeout);
guint64 timer_wheel_get_timer_expiration(TimerWheel *self, TWEntry *entry);

void timer_wheel_set_time(TimerWheel *self, guint64 new_now);
guint64 timer_wheel_get_time(TimerWheel *self);
void timer_wheel_expire_all(TimerWheel *self);
void timer_wheel_set_associated_data(TimerWheel *self, gpointer assoc_data, GDestroyNotify assoc_data_free);
gpointer timer_wheel_get_associated_data(TimerWheel *self);
TimerWheel *timer_wheel_new(void);
void timer_wheel_free(TimerWheel *self);


#endif
