/*
 * Copyright (c) 2002-2019 Balabit
 * Copyright (c) 1998-2019 Balázs Scheidler
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
#include "timeutils/zonedb.h"
#include "pathutils.h"
#include "reloc.h"

static const gchar *time_zone_path_list[] =
{
#ifdef PATH_TIMEZONEDIR
  PATH_TIMEZONEDIR,               /* search the user specified dir */
#endif
  SYSLOG_NG_PATH_PREFIX "/share/zoneinfo/", /* then local installation first */
  "/usr/share/zoneinfo/",         /* linux */
  "/usr/share/lib/zoneinfo/",     /* solaris, AIX */
  NULL,
};

static const gchar *time_zone_basedir = NULL;

const gchar *
get_time_zone_basedir(void)
{
  int i = 0;

  if (!time_zone_basedir)
    {
      for (i = 0; time_zone_path_list[i] != NULL; i++)
        {
          const gchar *candidate = get_installation_path_for(time_zone_path_list[i]);
          if (is_file_directory(candidate))
            {
              time_zone_basedir = candidate;
              break;
            }
        }
    }
  return time_zone_basedir;
}
