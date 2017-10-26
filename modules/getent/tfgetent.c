/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Gergely Nagy <algernon@balabit.hu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 */

#if defined(sun) || defined(__sun)
#define _POSIX_PTHREAD_SEMANTICS
#endif

#include "syslog-ng.h"
#include "logmsg/logmsg.h"
#include "plugin.h"
#include "plugin-types.h"
#include "cfg.h"
#include "parse-number.h"
#include "template/simple-function.h"
#include "compat/getent.h"

#include <grp.h>
#include <pwd.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>


typedef gboolean (*lookup_method)(gchar *key, gchar *member_name, GString *result);
typedef gboolean (*format_member)(gchar *member_name, gpointer member, GString *result);

typedef struct
{
  gchar *member_name;
  format_member format;
  size_t offset;
} formatter_map_t;

static gboolean
_getent_format_array(gchar *name_or_gid, gpointer members, GString *result)
{

  gchar **member_array = (gchar **)members;
  int i = 0;

  if (member_array[i])
    {
      g_string_append(result, member_array[i]);
      i++;
    }
  else
    return TRUE;

  while (member_array[i])
    {
      g_string_append(result, ",");
      g_string_append(result, member_array[i]);
      i++;
    }

  return TRUE;
}

static gboolean
_getent_format_string(gchar *member_name, gpointer member, GString *result)
{
  char *value = *(char **)member;

  g_string_append(result, value);
  return TRUE;
}

static gboolean
_getent_format_uid_gid(gchar *member_name, gpointer member, GString *result)
{
  if (strcmp(member_name, "uid") == 0)
    {
      uid_t u = *(uid_t *)member;

      g_string_append_printf(result, "%" G_GUINT64_FORMAT, (guint64)u);
    }
  else
    {
      gid_t g = *(gid_t *)member;

      g_string_append_printf(result, "%" G_GUINT64_FORMAT, (guint64)g);
    }

  return TRUE;
}

static int
_find_formatter(formatter_map_t *map, gchar *member_name)
{
  gint i = 0;

  while (map[i].member_name != NULL)
    {
      if (strcmp(map[i].member_name, member_name) == 0)
        return i;
      i++;
    }

  return -1;
}

#include "getent-protocols.c"
#include "getent-services.c"
#include "getent-group.c"
#include "getent-passwd.c"

static struct
{
  gchar *entity;
  lookup_method lookup;
} tf_getent_lookup_map[] =
{
  { "group", tf_getent_group },
  { "passwd", tf_getent_passwd },
  { "services", tf_getent_services },
  { "protocols", tf_getent_protocols },
  { NULL, NULL }
};

static lookup_method
tf_getent_find_lookup_method(gchar *entity)
{
  gint i = 0;

  while (tf_getent_lookup_map[i].entity != NULL)
    {
      if (strcmp(tf_getent_lookup_map[i].entity, entity) == 0)
        return tf_getent_lookup_map[i].lookup;
      i++;
    }
  return NULL;
}

static gboolean
tf_getent(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  lookup_method lookup;

  if (argc != 2 && argc != 3)
    {
      msg_error("$(getent) takes either two or three arguments",
                evt_tag_int("argc", argc),
                NULL);
      return FALSE;
    }

  lookup = tf_getent_find_lookup_method(argv[0]->str);
  if (!lookup)
    {
      msg_error("Unsupported $(getent) NSS service",
                evt_tag_str("service", argv[0]->str),
                NULL);
      return FALSE;
    }

  return lookup(argv[1]->str, (argc == 2) ? NULL : argv[2]->str, result);
}
TEMPLATE_FUNCTION_SIMPLE(tf_getent);

static Plugin getent_plugins[] =
{
  TEMPLATE_FUNCTION_PLUGIN(tf_getent, "getent"),
};

gboolean
getent_plugin_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, getent_plugins, G_N_ELEMENTS(getent_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "getent-plugin",
  .version = SYSLOG_NG_VERSION,
  .description = "The getent module provides getent template functions for syslog-ng.",
  .core_revision = VERSION_CURRENT_VER_ONLY,
  .plugins = getent_plugins,
  .plugins_len = G_N_ELEMENTS(getent_plugins),
};
