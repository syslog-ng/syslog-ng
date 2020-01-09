/*
 * Copyright (c) 2002-2019 One Identity
 * Copyright (c) 2019 Laszlo Budai <laszlo.budai@oneidentity.com>
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

#ifndef SIGNAL_SLOT_CONNECTOR_H_INCLUDED
#define SIGNAL_SLOT_CONNECTOR_H_INCLUDED

#include "syslog-ng.h"
#include "messages.h"

typedef struct _SignalSlotConnector SignalSlotConnector;

typedef const gchar *Signal;

#define STR(x) #x
#define SIGNAL(modul, signal, signal_param_type) \
  STR(modul) "::signal_" STR(signal) "(" STR(signal_param_type) ")"

#define CONNECT(connector, signal, slot) \
  signal_slot_connect(connector, signal, (Slot) slot);

#define DISCONNECT(connector, signal, slot) \
  signal_slot_disconnect(connector, signal, (Slot) slot);

#define EMIT(connector, signal, user_data) \
  signal_slot_emit(connector, signal, (gpointer) user_data);

typedef void (*Slot)(gpointer user_data);

void signal_slot_connect(SignalSlotConnector *self, Signal signal, Slot slot);
void signal_slot_disconnect(SignalSlotConnector *self, Signal signal, Slot slot);

void signal_slot_emit(SignalSlotConnector *self, Signal signal, gpointer user_data);

SignalSlotConnector *signal_slot_connector_new(void);
void signal_slot_connector_free(SignalSlotConnector *self);

#endif

