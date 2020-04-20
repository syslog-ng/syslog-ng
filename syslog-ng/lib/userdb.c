/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include "userdb.h"

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <stdlib.h>

gboolean
resolve_user(const char *user, gint *uid)
{
  struct passwd *pw;
  gchar *endptr;

  *uid = 0;
  if (!(*user))
    return FALSE;

  *uid = strtol(user, &endptr, 0);
  if (*endptr)
    {
      pw = getpwnam(user);
      if (!pw)
        return FALSE;

      *uid = pw->pw_uid;
    }
  return TRUE;
}

gboolean
resolve_group(const char *group, gint *gid)
{
  struct group *gr;
  gchar *endptr;

  *gid = 0;
  if (!(*group))
    return FALSE;

  *gid = strtol(group, &endptr, 0);
  if (*endptr)
    {
      gr = getgrnam(group);
      if (!gr)
        return FALSE;

      *gid = gr->gr_gid;
    }
  return TRUE;
}

gboolean
resolve_user_group(char *arg, gint *uid, gint *gid)
{
  char *user, *group;

  *uid = 0;
  user = strtok(arg, ":.");
  group = strtok(NULL, "");

  if (user && !resolve_user(user, uid))
    return FALSE;
  if (group && !resolve_group(group, gid))
    return FALSE;
  return TRUE;
}
