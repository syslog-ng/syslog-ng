/*
 * Copyright (c) 2010 Balabit
 * Copyright (c) 2010 Balázs Scheidler
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

int main(void)
{
  gint uid;
  gint gid;

  if (!resolve_user("root", &uid))
    return 1;

  if (!resolve_user("0", &uid) || uid != 0)
    return 1;

  if (!resolve_user("-1", &uid) || uid != -1)
    return 1;

  if (resolve_group("nincsily", &gid))
    return 1;

  if (!resolve_group("0", &gid) || gid != 0)
    return 1;

  if (!resolve_group("-1", &gid) || gid != -1)
    return 1;

  if (!resolve_group("sys", &gid))
    return 1;

  if (resolve_group("nincsily", &gid))
    return 1;
  return 0;
}
