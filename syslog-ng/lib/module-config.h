/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balázs Scheidler
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
#ifndef MODULE_CONFIG_H_INCLUDED
#define MODULE_CONFIG_H_INCLUDED 1

#include "syslog-ng.h"

typedef struct _ModuleConfig ModuleConfig;

/* This class encapsulates global (e.g. GlobalConfig) level settings for
 * modules.  It can be used to store state that is not related to a specific
 * LogPipe instance.  Such an example is the python block embedded in the
 * configuration used by the Python module.  */
struct _ModuleConfig
{
  /* init/deinit is hooked into configuration init/deinit */
  gboolean (*init)(ModuleConfig *s, GlobalConfig *cfg);
  void (*deinit)(ModuleConfig *s, GlobalConfig *cfg);
  void (*free_fn)(ModuleConfig *s);
};

gboolean module_config_init(ModuleConfig *s, GlobalConfig *cfg);
void module_config_deinit(ModuleConfig *s, GlobalConfig *cfg);
void module_config_free_method(ModuleConfig *s);
void module_config_free(ModuleConfig *s);

#endif
