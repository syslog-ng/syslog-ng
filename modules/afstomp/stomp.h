/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Viktor Tusa <tusa@balabit.hu>
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

#ifndef STOMP_H
#define STOMP_H

#include "gsocket.h"
#include <glib.h>

typedef struct stomp_connection
{
  int socket;
  GSockAddr *remote_sa;
  char *remote_ip;
} stomp_connection;

typedef struct stomp_frame
{
  char *command;
  GHashTable *headers;
  char *body;
  int body_length;
} stomp_frame;

void stomp_frame_init(stomp_frame *frame, const char *command, int command_len);
void stomp_frame_add_header(stomp_frame *frame, const char *name,
                            const char *value);
void stomp_frame_set_body(stomp_frame *frame, const char *body, int body_len);
int stomp_frame_deinit(stomp_frame *frame);

int stomp_connect(stomp_connection **connection_ref, char *hostname, int port);
int stomp_disconnect(stomp_connection **connection_ref);

int stomp_write(stomp_connection *connection, stomp_frame *frame);
int stomp_read(stomp_connection *connection, stomp_frame **frame);
int stomp_parse_frame(GString *data, stomp_frame *frame);
int stomp_receive_frame(stomp_connection *connection, stomp_frame *frame);

GString *create_gstring_from_frame(stomp_frame *frame);

#endif
