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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "misc.h"
#include "reloc.h"


GHashTable *configure_variables=NULL;
gchar *sysprefix = NULL;
/* Recursively expand the configure variables from the string orig.
 * We need this to allow proper relocation support of syslog-ng when the suite
 * doesn't installed under PATH_PREFIX, but into a path declared in the
 * SYSLOGNG_PREFIX environment variable.
 *
 * Returns a newly allocated string, which must be g_free'd after use.
 */

char *get_reloc_string(const char *orig)
{
  gchar *ppos = NULL;
  gchar *epos = NULL;
  gchar *res2, *prefix, *suffix, *replace, *res;
  gchar *confvar;
  gint prefixlen, suffixlen, confvarlen;

  if (sysprefix == NULL)
    if ((sysprefix = getenv("SYSLOGNG_PREFIX")) == NULL )
      sysprefix = PATH_PREFIX;

  if (!configure_variables)
    {
      configure_variables = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      g_hash_table_insert(configure_variables, "${prefix}", sysprefix);
      g_hash_table_insert(configure_variables, "${exec_prefix}", PATH_EXECPREFIX);
      g_hash_table_insert(configure_variables, "${libexecdir}", PATH_LIBEXECDIR);
      g_hash_table_insert(configure_variables, "${datarootdir}", PATH_DATAROOTDIR);
      g_hash_table_insert(configure_variables, "${datadir}", PATH_DATADIR);
      g_hash_table_insert(configure_variables, "${localstatedir}", PATH_LOCALSTATEDIR);
#ifdef PATH_TIMEZONEDIR
      g_hash_table_insert(configure_variables, "${timezonedir}", PATH_TIMEZONEDIR);
#endif
    }

  res = g_strdup(orig);
  ppos = strstr(res, "${");
  while ( ppos != NULL )
    {
      prefix=NULL;
      suffix=NULL;
      suffixlen=0;
      prefixlen=0;

      epos = strchr(ppos, '}');

      if ( epos == NULL ) {
        fprintf(stderr, "%s: Token error, missing '}' in string '%s'. Please re-compile syslog-ng with proper path variables.\n", __FILE__, res);
        exit(1);
      }
      confvarlen = (epos + 1) - ppos;
      confvar = g_strndup(res, confvarlen);

      replace = g_hash_table_lookup(configure_variables, (gconstpointer)confvar);

      if ( replace == NULL ){
        fprintf(stderr, "%s: Unknown configure variable: '%s' in string '%s'.\nPlease update %s or configure\n.", __FILE__, confvar, res, __FILE__);
        exit(1);
      }
      g_free(confvar);

      if ( strlen(epos + 1) > 0 ){
        suffixlen = strlen(epos + 2 );
        suffix = (epos + 2);
      }

      prefixlen = ppos - res;
      if ( prefixlen > 0 ) {
        prefix = g_strndup(res, prefixlen);
        res2 = g_strconcat(prefix, replace, "/", suffix, NULL);
        g_free(prefix);
      } else {
        res2 = g_strconcat(replace, "/", suffix, NULL);
      }

      g_free(res);
      res = res2;

      ppos = strstr(res, "${");
    }
  return res;
}
