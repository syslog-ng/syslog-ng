/*
 * Copyright (c) 2021 Balazs Scheidler <bazsi77@gmail.com>
 * Copyright (c) 2002-2018 Balabit
 * Copyright (c) 1998-2018 Balázs Scheidler
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
#include "timeutils/timeutils.h"
#include "timeutils/cache.h"
#include "apphook.h"

static void
_reset_timezone_apphook(gint type, gpointer user_data)
{
  invalidate_timeutils_cache();
}

void
timeutils_global_init(void)
{
  invalidate_timeutils_cache();

  /* We are using the STOPPED hook as the timezone related variables (tzname
   * and timezone) may be changed without locking by other threads.  At
   * AH_CONFIG_STOPPED those threads should have stopped already, so nothing
   * touches the global variables.  Hopefully. */
  register_application_hook(AH_CONFIG_STOPPED, _reset_timezone_apphook, NULL, AHM_RUN_REPEAT);
}
