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

#include "syslog-ng.h"
#include "reloc.h"

GlobalConfig *configuration;
int cfg_parser_debug;
#ifndef _MSC_VER
gchar *module_path;
#endif
gchar *default_modules = DEFAULT_MODULES;

gchar *path_prefix;
gchar *path_datadir;
gchar *path_sysconfdir;
gchar *path_pidfiledir;
gchar *path_patterndb_file;
gchar *cfgfilename;
gchar *persist_file;
gchar *ctlfilename;

gchar *qdisk_dir = NULL;

INITIALIZER(init_paths)
{
  /* initialize the SYSLOGNG_PREFIX-related variables */
  path_prefix = get_reloc_string(PATH_PREFIX);
  path_datadir = get_reloc_string(PATH_DATADIR);
  path_sysconfdir = get_reloc_string(PATH_SYSCONFDIR);
  path_pidfiledir = get_reloc_string(PATH_PIDFILEDIR);
  path_patterndb_file = get_reloc_string(PATH_PATTERNDB_FILE);
  cfgfilename = get_reloc_string(PATH_SYSLOG_NG_CONF);
  persist_file = get_reloc_string(PATH_PERSIST_CONFIG);
  ctlfilename = get_reloc_string(PATH_CONTROL_SOCKET);
#ifndef _MSC_VER
  module_path = get_reloc_string(MODULE_PATH);
#endif
}
