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

#include "syslog-ng.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int 
main(int argc, char *argv[])
{
#ifdef ENV_LD_LIBRARY_PATH
  {
    gchar *cur_ldlibpath;
    gchar ldlibpath[512];
#if _AIX
    const gchar *ldlibpath_name = "LIBPATH";
#else    
    const gchar *ldlibpath_name = "LD_LIBRARY_PATH";
#endif

    cur_ldlibpath = getenv(ldlibpath_name);
    snprintf(ldlibpath, sizeof(ldlibpath), "%s=%s%s%s", ldlibpath_name, ENV_LD_LIBRARY_PATH, cur_ldlibpath ? ":" : "", cur_ldlibpath ? cur_ldlibpath : "");
    putenv(ldlibpath);
  }
#endif
  execv(PATH_SYSLOGNG, argv);
  return 127;
}
