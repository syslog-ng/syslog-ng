/*
 * Copyright (c) 2002-2010 Balabit
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
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* Resolves the "${prefix}"/"${exec_prefix}" configure-variable placeholders
 * PATH_SYSLOGNG may contain, mirroring lib/reloc.c's path_resolver_resolve()
 * algorithm. Duplicated here (pure libc, no glib/libsyslog-ng calls) instead
 * of linking against get_installation_path_for(), so this wrapper can still
 * start when the real binary's shared libs aren't resolvable without
 * LD_LIBRARY_PATH set first. Falls back to the raw, unresolved path (the
 * pre-existing behavior) on any failure. */
static gboolean
resolve_configure_variable(const gchar *name, gchar *out, gsize out_size)
{
  const gchar *value = NULL;

  if (strcmp(name, "${prefix}") == 0)
    {
      value = getenv("SYSLOGNG_PREFIX");
      if (!value)
        value = SYSLOG_NG_PATH_PREFIX;
    }
  else if (strcmp(name, "${exec_prefix}") == 0)
    value = SYSLOG_NG_PATH_EXECPREFIX;

  if (!value)
    return FALSE;

  return snprintf(out, out_size, "%s", value) < (int) out_size;
}

static gboolean
resolve_path_syslogng(gchar *buf, gsize buf_size)
{
  gchar work[512];
  gchar *subst_start, *subst_end;

  if (snprintf(work, sizeof(work), "%s", PATH_SYSLOGNG) >= (int) sizeof(work))
    return FALSE;

  while ((subst_start = strstr(work, "${")) != NULL)
    {
      gchar varname[64], replacement[256], new_work[512];
      gsize varname_len;

      subst_end = strchr(subst_start, '}');
      if (!subst_end)
        return FALSE;

      varname_len = (gsize) (subst_end - subst_start) + 1;
      if (varname_len >= sizeof(varname))
        return FALSE;
      memcpy(varname, subst_start, varname_len);
      varname[varname_len] = '\0';

      if (!resolve_configure_variable(varname, replacement, sizeof(replacement)))
        return FALSE;

      if (snprintf(new_work, sizeof(new_work), "%.*s%s%s",
                   (int) (subst_start - work), work, replacement, subst_end + 1) >= (int) sizeof(new_work))
        return FALSE;

      memcpy(work, new_work, sizeof(work));
    }

  return snprintf(buf, buf_size, "%s", work) < (int) buf_size;
}

int
main(int argc, char *argv[])
{
  gchar resolved_path[512];
  const gchar *exec_path = PATH_SYSLOGNG;

  if (resolve_path_syslogng(resolved_path, sizeof(resolved_path)))
    exec_path = resolved_path;

#ifdef ENV_LD_LIBRARY_PATH
  {
    gchar *cur_ldlibpath;
    gchar ldlibpath[512];
#ifdef _AIX
    const gchar *ldlibpath_name = "LIBPATH";
#else
    const gchar *ldlibpath_name = "LD_LIBRARY_PATH";
#endif

    cur_ldlibpath = getenv(ldlibpath_name);
    snprintf(ldlibpath, sizeof(ldlibpath), "%s%s%s", ENV_LD_LIBRARY_PATH, cur_ldlibpath ? ":" : "",
             cur_ldlibpath ? cur_ldlibpath : "");
    setenv(ldlibpath_name, ldlibpath, TRUE);
  }
#endif
  execv(exec_path, argv);
  fprintf(stderr, "Unable to execute main syslog-ng binary from env-wrapper, path=%s, error=%s\n", exec_path,
          strerror(errno));
  return 127;
}
