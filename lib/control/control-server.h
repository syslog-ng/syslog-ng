/*
 * Copyright (c) 2002-2013 Balabit
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
#ifndef CONTROL_SERVER_H
#define CONTROL_SERVER_H

#include "syslog-ng.h"
#include "control.h"
#include <stdio.h>

#define MAX_CONTROL_LINE_LENGTH 4096

typedef struct _ControlServer ControlServer;
typedef struct _ControlConnection ControlConnection;

struct _ControlConnection
{
  GString *input_buffer;
  GString *output_buffer;
  gsize pos;
  ControlServer *server;
  int (*read)(ControlConnection *self, gpointer buffer, gsize size);
  int (*write)(ControlConnection *self, gpointer buffer, gsize size);
  void (*handle_input)(gpointer s);
  void (*handle_output)(gpointer s);
  void (*free_fn)(ControlConnection *self);
  struct
  {
    void (*start_watches)(ControlConnection *self);
    void (*update_watches)(ControlConnection *self);
    void (*stop_watches)(ControlConnection *self);
  } events;

};

struct _ControlServer
{
  gchar *control_socket_name;
  void (*free_fn)(ControlServer *self);
};


ControlServer *control_server_new(const gchar *path);

void control_server_start(ControlServer *self);
void control_server_free(ControlServer *self);
void control_server_init_instance(ControlServer *self, const gchar *path);


void control_connection_start_watches(ControlConnection *self);
void control_connection_update_watches(ControlConnection *self);
void control_connection_stop_watches(ControlConnection *self);
void control_connection_free(ControlConnection *self);
void control_connection_init_instance(ControlConnection *self, ControlServer *server);

#endif
