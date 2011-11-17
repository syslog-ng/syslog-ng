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

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "misc.h"
#include "reloc.h"

/* return the configuration variable
   use this function instead of PATH_... constants for relocatable binaries
   if the replacement must be applied, a new string is allocated -> don't use
   this for local variables, _only_ instead of global variables (assignment
   in initialization)
*/
char *get_reloc_string(const char *orig)
{
  gchar *sysprefix = getenv("SYSLOGNG_PREFIX");
  if (sysprefix == NULL)
    sysprefix = PATH_PREFIX;
  gchar *prefix = "${prefix}";
  gchar *ppos = strstr(orig, prefix);
  gchar *res = orig;
  if (ppos != NULL)
    {
      gint len1 = ppos - orig;
      gint len2 = strlen(sysprefix);
      gint len3 = strlen(orig) - (len1 + strlen(prefix));
      gint len = len1 + len2 + len3;
      res = (gchar *)g_malloc(len + 1);
      memcpy(res, orig, len1);
      memcpy(res + len1, sysprefix, len2);
      memcpy(res + len1 + len2, ppos + strlen(prefix), len3);
      res[len] = '\0';
    }

  return res;
}
