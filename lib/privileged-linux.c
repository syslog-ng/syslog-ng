/*
 * Copyright (c) 2002-2015 BalaBit IT Ltd, Budapest, Hungary
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

#include "privileged-linux.h"
#include "gprocess.h"

#include <errno.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <glib.h>

#ifndef PR_CAPBSET_READ

/* old glibc versions don't have PR_CAPBSET_READ, we define it to the
 * value as defined in newer versions. */

#define PR_CAPBSET_READ 23
#endif

static gboolean have_capsyslog = FALSE;

void
set_process_dumpable()
{
  gint rc;

  if (!prctl(PR_GET_DUMPABLE, 0, 0, 0, 0))
    {
      rc = prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
      if (rc < 0)
        g_process_message("Cannot set process to be dumpable; error='%s'", g_strerror(errno));
    }
}

void
set_keep_caps_flag(const gchar *caps)
{
  if (caps)
    prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
}

gboolean
check_syslog_cap()
{
  int ret;

  if (have_capsyslog)
    return TRUE;

  if (CAP_SYSLOG == -1)
    return FALSE;

  ret = prctl(PR_CAPBSET_READ, CAP_SYSLOG);
  if (ret == -1)
    return FALSE;

  ret = cap_from_name("cap_syslog", NULL);
  if (ret == -1)
    {
      g_process_message("CAP_SYSLOG seems to be supported by the system, but "
         "libcap can't parse it. Falling back to CAP_SYS_ADMIN!");
      return FALSE;
    }

  have_capsyslog = TRUE;
  return TRUE;
}
