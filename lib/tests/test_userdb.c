/*
 * Copyright (c) 2018 Balabit
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
#include <criterion/criterion.h>

Test(user_db, resolve_user_root)
{
  gint uid;

  /* On OSX the root user is disabled by default */
#ifndef __APPLE__
  cr_assert(resolve_user("root", &uid));
  cr_assert_eq(uid, 0);
#endif
  cr_assert(resolve_user("0", &uid));
  cr_assert_eq(uid, 0);
}

Test(user_db, resolve_non_existing_user)
{
  gint uid;

  cr_assert_not(resolve_user("nemtudom", &uid));
  cr_assert_not(resolve_user("", &uid));
}

Test(user_db, resolve_group)
{
  gint gid;

  cr_assert(resolve_group("0", &gid));
  cr_assert_eq(gid, 0);

  cr_assert(resolve_group("sys", &gid));

  cr_assert(resolve_group("-1", &gid));
  cr_assert_eq(gid, -1);
}

Test(user_db, resolve_non_existing_group)
{
  gint gid;

  cr_assert_not(resolve_group("nincsily", &gid));
  cr_assert_not(resolve_group("", &gid));
}

/* On OSX the root user is disabled by default */
#ifndef __APPLE__
Test(user_db, resolve_user_group)
{
  gint uid;
  gint gid;
  char str[] = "root:root";

  cr_assert(resolve_user_group(str, &uid, &gid));
  cr_assert_eq(uid, 0);
  cr_assert_eq(gid, 0);
}
#endif

Test(user_db, resolve_none_existing_user_group)
{
  gint uid;
  gint gid;
  char str[] = "nemtudom:nincsily";

  cr_assert_not(resolve_user_group(str, &uid, &gid));
}
