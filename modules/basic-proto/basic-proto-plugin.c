/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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

#include "cfg-parser.h"
#include "plugin.h"
#include "tlscontext.h"
#include "logproto.h"
#include "basic-proto.h"

/*
 * tls-framed client
 */
static LogProtoFactory tls_framed_client_proto =
{
  .create = &log_proto_framed_client_new_plugin,
  .construct_transport = NULL,
  .default_port = 6514
};

static LogProtoFactory *
tls_framed_client_proto_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &tls_framed_client_proto;
}


/*
 * tls-framed server
 */
static LogProtoFactory tls_framed_server_proto =
{
  .create = &log_proto_framed_server_new_plugin,
  .construct_transport = NULL,
  .default_port = 6514
};

static LogProtoFactory *
tls_framed_server_proto_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &tls_framed_server_proto;
}

/*
 * stream-framed client
 */

static LogProtoFactory stream_framed_client_proto =
{
  .create = &log_proto_framed_client_new_plugin,
  .construct_transport = NULL,
  .default_port = 601
};

static LogProtoFactory *
stream_framed_client_proto_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &stream_framed_client_proto;
}

/*
 * stream-framed server
 */
static LogProtoFactory stream_framed_server_proto =
{
  .create = &log_proto_framed_server_new_plugin,
  .construct_transport = NULL,
  .default_port = 601
};

static LogProtoFactory *
stream_framed_server_proto_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &stream_framed_server_proto;
}

/*
 * stream-newline client
 */
static LogProtoFactory stream_newline_client_proto =
{
  .create = &log_proto_text_client_new_plugin,
  .construct_transport = NULL,
  .default_port = 514
};

static LogProtoFactory *
stream_newline_client_proto_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &stream_newline_client_proto;
}

/*
 * stream-newline server
 */
static LogProtoFactory stream_newline_server_proto =
{
  .create = &log_proto_text_server_new_plugin,
  .construct_transport = NULL,
  .default_port = 514
};

static LogProtoFactory file_reader_proto =
{
  .create = &log_proto_file_reader_new_plugin
};

static LogProtoFactory *
stream_newline_server_proto_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &stream_newline_server_proto;
}

static LogProtoFactory *
file_reader_proto_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &file_reader_proto;
}

/*
 * record client
 */
static LogProtoFactory record_client_proto =
{
  .create = &log_proto_record_client_new_plugin,
  .construct_transport = NULL
};

static LogProtoFactory *
record_client_proto_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &record_client_proto;
}

/*
 * record server
 */
static LogProtoFactory record_server_proto =
{
  .create = &log_proto_record_server_new_plugin,
  .construct_transport = NULL
};

static LogProtoFactory *
record_server_proto_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &record_server_proto;
}

/*
 * dgram client
 */
static LogProtoFactory dgram_client_proto =
{
  .create = &log_proto_dgram_client_new_plugin,
  .construct_transport = NULL,
  .default_port = 514
};

static LogProtoFactory *
dgram_client_proto_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &dgram_client_proto;
}

/*
 * dgram server
 */
static LogProtoFactory dgram_server_proto =
{
  .create = &log_proto_dgram_server_new_plugin,
  .construct_transport = NULL,
  .default_port = 514
};

static LogProtoFactory *
dgram_server_proto_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &dgram_server_proto;
}

/*
 * File writer
 */
static LogProtoFactory file_writer_client_proto =
{
  .create = &log_proto_file_writer_new_plugin,
  .construct_transport = NULL
};

static LogProtoFactory *
file_writer_client_proto_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &file_writer_client_proto;
}

static Plugin proto_plugins[] =
{
  {
  .type = LL_CONTEXT_CLIENT_PROTO,
  .name = "tls-framed",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) tls_framed_client_proto_construct,
  },
  {
  .type = LL_CONTEXT_SERVER_PROTO,
  .name = "tls-framed",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) tls_framed_server_proto_construct,
  },
  {
  .type = LL_CONTEXT_CLIENT_PROTO,
  .name = "tls-newline",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) stream_newline_client_proto_construct,
  },
  {
  .type = LL_CONTEXT_SERVER_PROTO,
  .name = "tls-newline",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) stream_newline_server_proto_construct,
  },
  {
  .type = LL_CONTEXT_CLIENT_PROTO,
  .name = "stream-framed",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) stream_framed_client_proto_construct,
  },
  {
  .type = LL_CONTEXT_SERVER_PROTO,
  .name = "stream-framed",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) stream_framed_server_proto_construct,
  },
  {
  .type = LL_CONTEXT_CLIENT_PROTO,
  .name = "stream-newline",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) stream_newline_client_proto_construct,
  },
  {
  .type = LL_CONTEXT_SERVER_PROTO,
  .name = "stream-newline",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) stream_newline_server_proto_construct,
  },
  {
  .type = LL_CONTEXT_SERVER_PROTO,
  .name = "file-reader",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) file_reader_proto_construct,
  },
  {
  .type = LL_CONTEXT_CLIENT_PROTO,
  .name = "record",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) record_client_proto_construct,
  },
  {
  .type = LL_CONTEXT_SERVER_PROTO,
  .name = "record",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) record_server_proto_construct,
  },
  {
  .type = LL_CONTEXT_CLIENT_PROTO,
  .name = "dgram",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) dgram_client_proto_construct,
  },
  {
  .type = LL_CONTEXT_SERVER_PROTO,
  .name = "dgram",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) dgram_server_proto_construct,
  },
  {
  .type = LL_CONTEXT_CLIENT_PROTO,
  .name = "file-writer",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) file_writer_client_proto_construct,
  }
};

gboolean
basic_proto_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, proto_plugins, G_N_ELEMENTS(proto_plugins));
  return TRUE;
}

#ifndef STATIC
const ModuleInfo module_info =
{
  .canonical_name = "basic-proto",
  .version = VERSION,
  .description = "The proto module provides base communication protocol managers",
  .core_revision = SOURCE_REVISION,
  .plugins = proto_plugins,
  .plugins_len = G_N_ELEMENTS(proto_plugins),
};
#endif
