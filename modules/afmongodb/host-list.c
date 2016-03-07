/*
 * Copyright (c) 2016 Balabit
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
 *
 */

#include "host-list.h"

typedef struct _MongoDBHostPort
{
  char *host;
  gint port;
} MongoDBHostPort;

static MongoDBHostPort *
_new_host_port(const char *host, gint port)
{
  MongoDBHostPort *hp = g_new0(MongoDBHostPort, 1);
  hp->host = g_strdup(host);
  hp->port = port;
  return hp;
}

static void
_free_host_port(gpointer data)
{
  MongoDBHostPort *hp = (MongoDBHostPort *)data;
  g_free(hp->host);
  hp->host = NULL;
  g_free(hp);
}

gboolean
host_list_append(HostList **list, const char *host, gint port)
{
  if (!list)
    return FALSE;
  *list = g_list_append(*list, _new_host_port(host, port));
  return TRUE;
}

gboolean
host_list_iterate(const HostList *host_list, HostListProcessor processor, gpointer user_data)
{
  for (const GList *iterator = host_list; iterator; iterator = iterator->next)
    {
      const MongoDBHostPort *hp = (const MongoDBHostPort *)iterator->data;
      if (!processor(user_data, hp->host, hp->port))
        return FALSE;
    }
  return TRUE;
}

void
host_list_free(HostList *host_list)
{
  g_list_free_full(host_list, (GDestroyNotify)&_free_host_port);
}
