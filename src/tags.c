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

#include "tags.h"
#include "messages.h"

typedef struct _LogTag
{
  guint id;
  gchar *name;
} LogTag;

static LogTag *log_tags_list = NULL;
static GHashTable *log_tags_hash = NULL;
static guint32 log_tags_num = 0;
static GStaticMutex log_tags_lock = G_STATIC_MUTEX_INIT;


/*
 * log_tags_get_by_name
 *
 * Lookup a tag id by it's name. If the tag is seen for the first time
 * the next tag id is assigned and the tag is added to the list.
 *
 * The function returns the tag id associated with the name.
 *
 * @name:   the name of the tag
 *
 */
guint
log_tags_get_by_name(const gchar *name)
{
  guint id;

  g_static_mutex_lock(&log_tags_lock);

  id = GPOINTER_TO_UINT(g_hash_table_lookup(log_tags_hash, name)) - 1;
  if (id == -1)
    {
      id = log_tags_num;
      log_tags_list = g_renew(LogTag, log_tags_list, ++log_tags_num);
      log_tags_list[id].id = id;
      log_tags_list[id].name = g_strdup(name);
      g_hash_table_insert(log_tags_hash, log_tags_list[id].name, GUINT_TO_POINTER(log_tags_list[id].id + 1));
    }

  g_static_mutex_unlock(&log_tags_lock);

  return id;
}

/*
 * log_tag_get_by_id
 *
 * Lookup a tag name by it's id. If the id is invalid
 * NULL is returned, otherwise a gchar * is returned
 * pointing to the name of the tag.
 *
 * The returned pointer should not be freed.
 *
 * @id:     the tag id to lookup
 *
 */
gchar *
log_tags_get_by_id(guint id)
{
  gchar *name = NULL;

  g_static_mutex_lock(&log_tags_lock);

  if (id < log_tags_num)
    name = log_tags_list[id].name;

  g_static_mutex_unlock(&log_tags_lock);

  return name;
}

void
log_tags_init(void)
{
  log_tags_hash = g_hash_table_new(g_str_hash, g_str_equal);
}

void
log_tags_deinit(void)
{
  gint i;

  g_static_mutex_lock(&log_tags_lock);
  g_hash_table_destroy(log_tags_hash);

  for (i = 0; i < log_tags_num; i++)
    g_free(log_tags_list[i].name);

  g_free(log_tags_list);
}

