/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#ifndef PLUGIN_H_INCLUDED
#define PLUGIN_H_INCLUDED

#include "cfg-parser.h"
#include "cfg-grammar.h"

typedef struct _Plugin Plugin;

struct _Plugin
{
  gint type;
  const gchar *name;
  CfgParser *parser;
  void (*setup_context)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name);
  gpointer (*construct)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name);
};

/* instantiate a new plugin */
Plugin *plugin_find(GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name);
gpointer plugin_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name);
gpointer plugin_parse_config(Plugin *plugin, GlobalConfig *cfg, YYLTYPE *yylloc);


/* plugin side API */

/* helper macros for template function plugins */
#define TEMPLATE_FUNCTION(x) \
  static LogTemplateFunction \
  x ## _construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name) \
  {                                                                                            \
    return x;                                                                                  \
  }

#define TEMPLATE_FUNCTION_PLUGIN(x, tf_name) \
  {                                     \
    .type = LL_CONTEXT_TEMPLATE_FUNC,   \
    .name = tf_name,                    \
    .construct = x ## _construct,       \
  }

void plugin_register(GlobalConfig *cfg, Plugin *p, gint number);
gboolean plugin_load_module(const gchar *module_name, GlobalConfig *cfg, CfgArgs *args);

#endif
