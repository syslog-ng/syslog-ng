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
#include "logproto-dgram-server.h"
#include "logproto-text-client.h"
#include "logproto-text-server.h"
#include "logproto-indented-multiline-server.h"
#include "logproto-framed-client.h"
#include "logproto-framed-server.h"
#include "plugin.h"
#include "plugin-types.h"

/* This module defines various core-implemented LogProto implementations as
 * plugins, so that modules may find them, dynamically based on their plugin
 * name */

DEFINE_LOG_PROTO_SERVER(log_proto_dgram);
DEFINE_LOG_PROTO_CLIENT(log_proto_text);
DEFINE_LOG_PROTO_SERVER(log_proto_text);
DEFINE_LOG_PROTO_SERVER(log_proto_indented_multiline);
DEFINE_LOG_PROTO_CLIENT(log_proto_framed);
DEFINE_LOG_PROTO_SERVER(log_proto_framed);

static Plugin framed_server_plugins[] =
{
  /* there's no separate client side for the 'dgram' transport */
  LOG_PROTO_CLIENT_PLUGIN(log_proto_text, "dgram"),
  LOG_PROTO_SERVER_PLUGIN(log_proto_dgram, "dgram"),
  LOG_PROTO_CLIENT_PLUGIN(log_proto_text, "text"),
  LOG_PROTO_SERVER_PLUGIN(log_proto_text, "text"),
  LOG_PROTO_SERVER_PLUGIN(log_proto_indented_multiline, "indented-multiline"),
  LOG_PROTO_CLIENT_PLUGIN(log_proto_framed, "framed"),
  LOG_PROTO_SERVER_PLUGIN(log_proto_framed, "framed"),
};

void
log_proto_register_builtin_plugins(PluginContext *context)
{
  plugin_register(context, framed_server_plugins, G_N_ELEMENTS(framed_server_plugins));
}
