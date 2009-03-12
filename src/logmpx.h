/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
  
#ifndef LOGMPX_H_INCLUDED
#define LOGMPX_H_INCLUDED

#include "logpipe.h"

/* indicates that the multiplexed paths are independend, they can
 * potentically be processed by independent threads */
#define PIF_MPX_INDEP_PATHS       0x0100
/* the given multiplexer is a break in flow control */
#define PIF_MPX_FLOW_CTRL_BARRIER 0x0200

/**
 * This class encapsulates a fork of the message pipe-line. It receives
 * messages via its queue() method and forwards them to its list of
 * next_hops in addition to the standard pipe_next next-hop already provided
 * by LogPipe.
 *
 * This object is used for example for each source to send messages to all
 * log pipelines that refer to the source.
 **/
typedef struct _LogMultiplexer
{
  LogPipe super;
  GPtrArray *next_hops;
  gboolean fallback_exists;
} LogMultiplexer;

LogMultiplexer *log_multiplexer_new(guint32 flags);
void log_multiplexer_add_next_hop(LogMultiplexer *self, LogPipe *next_hop);


#endif
