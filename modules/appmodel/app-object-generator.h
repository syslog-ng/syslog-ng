/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balazs Scheidler <balazs.scheidler@balabit.com>
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

#ifndef APPMODEL_APP_OBJECT_GENERATOR_H_INCLUDED
#define APPMODEL_APP_OBJECT_GENERATOR_H_INCLUDED

#include "plugin.h"

typedef struct _AppObjectGenerator AppObjectGenerator;

struct _AppObjectGenerator
{
  CfgBlockGenerator super;
  gboolean (*parse_arguments)(AppObjectGenerator *self, CfgArgs *args, const gchar *reference);
  void (*generate_config)(AppObjectGenerator *self, GlobalConfig *cfg, GString *result);
  const gchar *included_apps;
  const gchar *excluded_apps;
  gboolean is_parsing_enabled;
};

gboolean app_object_generator_is_application_included(AppObjectGenerator *self, const gchar *app_name);
gboolean app_object_generator_is_application_excluded(AppObjectGenerator *self, const gchar *app_name);

gboolean app_object_generator_parse_arguments_method(AppObjectGenerator *self, CfgArgs *args, const gchar *reference);
void app_object_generator_init_instance(AppObjectGenerator *self, gint context, const gchar *name);


#endif
