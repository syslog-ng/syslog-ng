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
#include "messages.h"

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

void
setup_caps()
{
  if (check_syslog_cap())
    g_process_set_caps(BASE_CAPS_WITH_SYSLOG_CAP);
  else
    g_process_set_caps(BASE_CAPS_WITH_SYS_ADMIN_CAP);
}

static gint
__set_caps(cap_t caps, gchar *error_message)
{
  gchar *cap_text;
  gint rc;

  rc = cap_set_proc(caps);
  if (rc == -1)
    {
      cap_text = cap_to_text(caps, NULL);
      g_process_message(error_message, "caps", cap_text, "errno", errno);
      cap_free(cap_text);
    }

  return rc;
}

/**
 * Change the current capset to the value specified by the user.  causes the
 * startup process to fail if this function returns FALSE, but we only do
 * this if the capset cannot be parsed, otherwise a failure changing the
 * capabilities will not result in failure
 *
 * Returns: TRUE to indicate success
 **/
gboolean
setup_permitted_caps(const gchar *caps)
{
  cap_t cap;

  if (caps)
    {
      cap = cap_from_text(caps);
      if (!cap)
        {
          g_process_message("Error parsing capabilities: %s", caps);
          return FALSE;
        }

      if (cap_set_proc(cap) == -1)
        {
          g_process_message("Error setting permitted capabilities, capability management is disabled");
          g_process_set_caps(NULL);
        }

      cap_free(cap);
    }
  return TRUE;
}

gint
restore_caps(cap_t caps)
{
  if (!g_process_get_caps())
    return 0;

  return __set_caps(caps, "Error managing capability set, restore original capabilities returned an error");
}

gint
raise_caps(const gchar* new_caps, cap_t *old_caps)
{
  if (!g_process_get_caps())
    return 0;

  gchar str[1024];
  g_snprintf(str, sizeof(str), "%s%s", g_process_get_caps(), new_caps);

  *old_caps = cap_get_proc();
  return __set_caps(cap_from_text(str), "Error managing capability set, raise a new capability sets returned an error");
}

static inline const gchar *
__get_privileged_caps()
{
  if (check_syslog_cap())
    return SYSLOG_CAPS;
  else
    return SYSADMIN_CAPS;
}

static inline const gchar *
__get_ro_file_caps(const FileOpenOptions *open_opts)
{
  if (open_opts->needs_privileges)
    return __get_privileged_caps();
  else
    return OPEN_AS_READ_CAPS;
}

static inline const gchar *
__get_rw_file_caps(const FileOpenOptions *open_opts)
{
  return OPEN_AS_READ_AND_WRITE_CAPS;
}

const gchar *
get_open_file_caps(const FileOpenOptions *open_opts)
{
  if (!g_process_get_caps())
    return NULL;

  if ((open_opts->open_flags & O_WRONLY) == O_WRONLY ||
      (open_opts->open_flags & O_RDWR) == O_RDWR)
    return __get_rw_file_caps(open_opts);

  if ((open_opts->open_flags & O_RDONLY) == O_RDONLY)
    return __get_ro_file_caps(open_opts);

  return NULL;
}
