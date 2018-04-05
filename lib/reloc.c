/*
 * Copyright (c) 2002-2013 Balabit
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

#include "reloc.h"
#include "cache.h"

#include <stdlib.h>
#include <string.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Cache *path_cache;

typedef struct _PathResolver
{
  CacheResolver super;
  GHashTable *configure_variables;
} PathResolver;


void
path_resolver_add_configure_variable(CacheResolver *s, const gchar *name, const gchar *value)
{
  PathResolver *self = (PathResolver *) s;

  g_hash_table_insert(self->configure_variables, g_strdup(name), g_strdup(value));
}

static void
path_resolver_populate_configure_variables(PathResolver *self, const gchar *sysprefix)
{
  path_resolver_add_configure_variable(&self->super, "${prefix}", sysprefix);
  path_resolver_add_configure_variable(&self->super, "${exec_prefix}", SYSLOG_NG_PATH_EXECPREFIX);
  path_resolver_add_configure_variable(&self->super, "${libexecdir}", SYSLOG_NG_PATH_LIBEXECDIR);
  path_resolver_add_configure_variable(&self->super, "${datarootdir}", SYSLOG_NG_PATH_DATAROOTDIR);
  path_resolver_add_configure_variable(&self->super, "${datadir}", SYSLOG_NG_PATH_DATADIR);
  path_resolver_add_configure_variable(&self->super, "${localstatedir}", SYSLOG_NG_PATH_LOCALSTATEDIR);
}



/* NOTE: we really don't have an easy way to handle errors, as installation
 * paths are resolved quite early while syslog-ng starts up.  The best is to
 * abort, as an error is either a problem in the configure variables OR a
 * coding mistake. For now, we desperately try to write an error message to
 * stderr and then abort the process. */

#define handle_fatal_error(fmt, ...) \
    do {            \
      fprintf(stderr, fmt, __VA_ARGS__);    \
      g_assert_not_reached();       \
    } while (0)

gpointer
path_resolver_resolve(CacheResolver *s, const gchar *orig)
{
  PathResolver *self = (PathResolver *) s;
  gchar *subst_start = NULL;
  gchar *subst_end = NULL;
  gchar *new_value, *prefix, *suffix, *replacement, *value;
  gchar *confvar;
  gint prefix_len, confvar_len;


  /* Find variable enclosed within ${...}, replace with the value in
   * configure_variables.  Repeat until no more substitutions can be made.
   * */

  value = g_strdup(orig);
  subst_start = strstr(value, "${");
  while (subst_start != NULL)
    {
      prefix = NULL;
      prefix_len = 0;
      suffix = NULL;

      subst_end = strchr(subst_start, '}');

      if (subst_end == NULL)
        handle_fatal_error("Relocation resolution error: missing '}' in string '%s'. Please re-compile syslog-ng with proper path variables.\n",
                           value);

      /* extract confvar */

      confvar_len = (subst_end + 1) - subst_start;
      confvar = g_strndup(subst_start, confvar_len);

      /* lookup replacement */

      replacement = g_hash_table_lookup(self->configure_variables, confvar);

      if (replacement == NULL)
        handle_fatal_error("Relocation resolution error: Unknown configure variable: '%s' in string '%s'.\nPlease re-compile syslog-ng with proper path variables.\n",
                           confvar, value);

      g_free(confvar);

      /* replace confvar with replacement */

      suffix = (subst_end + 1);

      prefix_len = subst_start - value;
      prefix = g_strndup(value, prefix_len);

      /* construct the new value */
      new_value = g_strconcat(prefix, replacement, suffix, NULL);
      g_free(prefix);

      /* commit the new value */
      g_free(value);
      value = new_value;

      subst_start = strstr(value, "${");
    }
  return value;
}

static void
path_resolver_free(CacheResolver *s)
{
  PathResolver *self = (PathResolver *) s;

  g_hash_table_unref(self->configure_variables);
}

CacheResolver *
path_resolver_new(const gchar *sysprefix)
{
  PathResolver *self = g_new0(PathResolver, 1);

  self->super.resolve_elem = path_resolver_resolve;
  self->super.free_elem = g_free;
  self->super.free_fn = path_resolver_free;
  self->configure_variables = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  path_resolver_populate_configure_variables(self, sysprefix);

  return &self->super;
}

static const gchar *
lookup_sysprefix(void)
{
  gchar *v;

  v = getenv("SYSLOGNG_PREFIX");
  if (v)
    return v;
  return SYSLOG_NG_PATH_PREFIX;
}

const gchar *
get_installation_path_for(const gchar *template)
{
  if (!path_cache)
    path_cache = cache_new(path_resolver_new(lookup_sysprefix()));
  return cache_lookup(path_cache, template);
}

void
reloc_deinit(void)
{
  if (path_cache)
    {
      cache_free(path_cache);
      path_cache = NULL;
    }
}
