/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "plugin.h"
#include "template/templates.h"
#include "filter/filter-expr.h"
#include "filter/filter-expr-parser.h"
#include "cfg.h"
#include "parse-number.h"
#include "str-format.h"
#include "plugin-types.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* in order to avoid having to declare all construct functions, we
 * include them all here. If it causes compilation times to increase
 * drastically, we should probably make them into separate compilation
 * units. (Bazsi) */
#include "numeric-funcs.c"
#include "str-funcs.c"
#include "cond-funcs.c"
#include "ip-funcs.c"
#include "misc-funcs.c"

static Plugin basicfuncs_plugins[] =
{
  /* cond-funcs */
  TEMPLATE_FUNCTION_PLUGIN(tf_grep, "grep"),
  TEMPLATE_FUNCTION_PLUGIN(tf_if, "if"),

  /* str-funcs */
  TEMPLATE_FUNCTION_PLUGIN(tf_echo, "echo"),
  TEMPLATE_FUNCTION_PLUGIN(tf_length, "length"),
  TEMPLATE_FUNCTION_PLUGIN(tf_substr, "substr"),
  TEMPLATE_FUNCTION_PLUGIN(tf_strip, "strip"),
  TEMPLATE_FUNCTION_PLUGIN(tf_sanitize, "sanitize"),
  TEMPLATE_FUNCTION_PLUGIN(tf_lowercase, "lowercase"),
  TEMPLATE_FUNCTION_PLUGIN(tf_uppercase, "uppercase"),
  TEMPLATE_FUNCTION_PLUGIN(tf_replace_delimiter, "replace-delimiter"),

  /* numeric-funcs */
  TEMPLATE_FUNCTION_PLUGIN(tf_num_plus, "+"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_minus, "-"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_multi, "*"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_div, "/"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_mod, "%"),

  /* ip-funcs */
  TEMPLATE_FUNCTION_PLUGIN(tf_ipv4_to_int, "ipv4-to-int"),
  TEMPLATE_FUNCTION_PLUGIN(tf_indent_multi_line, "indent-multi-line"),

  /* misc funcs */
  TEMPLATE_FUNCTION_PLUGIN(tf_context_length, "context-length"),
  TEMPLATE_FUNCTION_PLUGIN(tf_env, "env"),
};

gboolean
basicfuncs_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, basicfuncs_plugins, G_N_ELEMENTS(basicfuncs_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "basicfuncs",
  .version = VERSION,
  .description = "The basicfuncs module provides various template functions for syslog-ng.",
  .core_revision = SOURCE_REVISION,
  .plugins = basicfuncs_plugins,
  .plugins_len = G_N_ELEMENTS(basicfuncs_plugins),
};
