/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#ifndef TRANSPORT_MAPPER_UNIX_H_INCLUDED
#define TRANSPORT_MAPPER_UNIX_H_INCLUDED

#include "transport-mapper.h"

typedef struct _TransportMapperUnix TransportMapperUnix;

TransportMapper *transport_mapper_unix_dgram_new(void);
TransportMapper *transport_mapper_unix_stream_new(void);
void transport_mapper_unix_set_pass_unix_credentials(TransportMapper *self, gboolean pass);

#endif
