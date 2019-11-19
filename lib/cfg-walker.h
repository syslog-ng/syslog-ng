/*
 * Copyright (c) 2019 Balabit
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

#ifndef CFG_WALKER_H_INCLUDED
#define CFG_WALKER_H_INCLUDED

#include "syslog-ng.h"

typedef enum
{
  ARC_TYPE_PIPE_NEXT,
  ARC_TYPE_NEXT_HOP
} ArcType;

typedef struct
{
  LogPipe *from;
  LogPipe *to;
  ArcType arc_type;
} Arc;

void cfg_walker_get_graph(GPtrArray *start_nodes, GHashTable **nodes, GHashTable **arcs);
Arc *arc_new(LogPipe *from, LogPipe *to, ArcType arc_type);
void arc_free(Arc *self);

#endif
