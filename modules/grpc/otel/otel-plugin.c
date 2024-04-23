/*
 * Copyright (c) 2023 Attila Szakacs
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


#include "cfg-parser.h"
#include "plugin.h"
#include "plugin-types.h"
#include "filterx/object-otel.h"
#include "otel-logmsg-handles.h"
#include "protos/apphook.h"


extern CfgParser otel_parser;

static Plugin otel_plugins[] =
{
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "opentelemetry",
    .parser = &otel_parser
  },
  {
    .type = LL_CONTEXT_PARSER,
    .name = "opentelemetry",
    .parser = &otel_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "opentelemetry",
    .parser = &otel_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "syslog_ng_otlp",
    .parser = &otel_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "syslog_ng_otlp",
    .parser = &otel_parser,
  },
  {
    .type = LL_CONTEXT_FILTERX_ENUM,
    .name = "otel",
    .construct = grpc_otel_filterx_enum_construct,
  },
  {
    .type = LL_CONTEXT_FILTERX_SIMPLE_FUNC,
    .name = "otel_logrecord",
    .construct = grpc_otel_filterx_logrecord_contruct_new,
  },
  {
    .type = LL_CONTEXT_FILTERX_SIMPLE_FUNC,
    .name = "otel_resource",
    .construct = grpc_otel_filterx_resource_construct_new,
  },
  {
    .type = LL_CONTEXT_FILTERX_SIMPLE_FUNC,
    .name = "otel_scope",
    .construct = grpc_otel_filterx_scope_construct_new,
  },
  {
    .type = LL_CONTEXT_FILTERX_SIMPLE_FUNC,
    .name = "otel_kvlist",
    .construct = grpc_otel_filterx_kvlist_construct_new,
  },
  {
    .type = LL_CONTEXT_FILTERX_SIMPLE_FUNC,
    .name = "otel_array",
    .construct = grpc_otel_filterx_array_construct_new,
  },
};

gboolean
otel_module_init(PluginContext *context, CfgArgs *args)
{
  otel_filterx_objects_global_init();
  plugin_register(context, otel_plugins, G_N_ELEMENTS(otel_plugins));
  grpc_register_global_initializers();
  otel_logmsg_handles_global_init();

  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "otel",
  .version = SYSLOG_NG_VERSION,
  .description = "OpenTelemetry plugins",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = otel_plugins,
  .plugins_len = G_N_ELEMENTS(otel_plugins),
};
