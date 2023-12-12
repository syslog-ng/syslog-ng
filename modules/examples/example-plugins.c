/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 László Várady <laszlo.varady@balabit.com>
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
#include "filterx/example-filterx-func/example-filterx-func-plugin.h"

extern CfgParser msg_generator_parser;

#ifdef SYSLOG_NG_HAVE_GETRANDOM
extern CfgParser threaded_random_generator_parser;
#endif

#if SYSLOG_NG_ENABLE_CPP
extern CfgParser random_choice_generator_parser;
#endif

extern CfgParser threaded_diskq_source_parser;

extern CfgParser http_test_slots_parser;

extern CfgParser tls_test_validation_parser;

extern CfgParser example_destination_parser;

static Plugin example_plugins[] =
{
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "example_msg_generator",
    .parser = &msg_generator_parser,
  },
#ifdef SYSLOG_NG_HAVE_GETRANDOM
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "example_random_generator",
    .parser = &threaded_random_generator_parser,
  },
#endif
#if SYSLOG_NG_ENABLE_CPP
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "random_choice_generator",
    .parser = &random_choice_generator_parser,
  },
#endif
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "example_diskq_source",
    .parser = &threaded_diskq_source_parser,
  },
  {
    .type = LL_CONTEXT_INNER_DEST,
    .name = "http_test_slots",
    .parser = &http_test_slots_parser
  },
  {
    .type = LL_CONTEXT_INNER_DEST,
    .name = "tls_test_validation",
    .parser = &tls_test_validation_parser
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "example_destination",
    .parser = &example_destination_parser
  },
  {
    .type = LL_CONTEXT_FILTERX_FUNC,
    .name = "example_echo",
    .construct = example_filterx_func_construct_echo,
  },
};

gboolean
examples_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, example_plugins, G_N_ELEMENTS(example_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "examples",
  .version = SYSLOG_NG_VERSION,
  .description = "Example modules",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = example_plugins,
  .plugins_len = G_N_ELEMENTS(example_plugins),
};
