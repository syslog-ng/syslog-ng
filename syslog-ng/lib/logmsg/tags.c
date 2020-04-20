/*
 * Copyright (c) 2009-2013 Balabit
 * Copyright (c) 2009 Marton Illes
 * Copyright (c) 2009-2013 Balázs Scheidler
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
#include "stats/stats-registry.h"
#include "apphook.h"

typedef struct _LogTag
{
  LogTagId id;
  gchar *name;
  StatsCounterItem *counter;
} LogTag;

static LogTag *log_tags_list = NULL;
static GHashTable *log_tags_hash = NULL;
static guint32 log_tags_num = 0;
static guint32 log_tags_list_size = 4;
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
LogTagId
log_tags_get_by_name(const gchar *name)
{
  /* If log_tags_hash() is NULL, this unit is already deinitialized
     but other thread may refer the tag structure.

     If name is empty, it is an extremal element.

     In both cases the return value is 0.
   */
  guint id;

  g_assert(log_tags_hash != NULL);

  g_static_mutex_lock(&log_tags_lock);

  id = GPOINTER_TO_UINT(g_hash_table_lookup(log_tags_hash, name)) - 1;
  if (id == 0xffffffff)
    {
      if (log_tags_num < LOG_TAGS_MAX - 1)
        {
          id = log_tags_num++;
          if (id == log_tags_list_size)
            {
              log_tags_list_size *= 2;
              log_tags_list = g_renew(LogTag, log_tags_list, log_tags_list_size);
            }
          log_tags_list[id].id = id;
          log_tags_list[id].name = g_strdup(name);
          log_tags_list[id].counter = NULL;

          /* NOTE: stats-level may not be set for calls that happen during
           * config file parsing, those get fixed up by
           * log_tags_reinit_stats() below */

          stats_lock();
          StatsClusterKey sc_key;
          stats_cluster_logpipe_key_set(&sc_key, SCS_TAG, name, NULL );
          stats_register_counter(3, &sc_key, SC_TYPE_PROCESSED, &log_tags_list[id].counter);
          stats_unlock();

          g_hash_table_insert(log_tags_hash, log_tags_list[id].name, GUINT_TO_POINTER(log_tags_list[id].id + 1));
        }
      else
        id = 0;
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
const gchar *
log_tags_get_by_id(LogTagId id)
{
  gchar *name = NULL;

  g_static_mutex_lock(&log_tags_lock);

  if (id < log_tags_num)
    name = log_tags_list[id].name;

  g_static_mutex_unlock(&log_tags_lock);

  return name;
}

void
log_tags_inc_counter(LogTagId id)
{
  g_static_mutex_lock(&log_tags_lock);

  if (id < log_tags_num)
    stats_counter_inc(log_tags_list[id].counter);

  g_static_mutex_unlock(&log_tags_lock);
}

void
log_tags_dec_counter(LogTagId id)
{
  /* Reader lock because the log_tag_list is not written */
  g_static_mutex_lock(&log_tags_lock);

  if (id < log_tags_num)
    stats_counter_dec(log_tags_list[id].counter);

  g_static_mutex_unlock(&log_tags_lock);
}

/*
 * NOTE: this is called at cfg_init() time to update the set of counters we
 * have.  If stats-level is decreased, we should unregister everything we
 * had earlier. If increased we need to register them again.
 *
 * log_tags_get_by_name() will also try to register the counter for calls
 * that are _after_ cfg_init().  Early calls to log_tags_get_by_name() will
 * not see a proper stats-level() in the global variable here.  Those will
 * get handled by this function.
 */
void
log_tags_reinit_stats(void)
{
  gint id;

  stats_lock();

  for (id = 0; id < log_tags_num; id++)
    {
      const gchar *name = log_tags_list[id].name;
      StatsClusterKey sc_key;
      stats_cluster_logpipe_key_set(&sc_key, SCS_TAG, name, NULL );

      if (stats_check_level(3))
        stats_register_counter(3, &sc_key, SC_TYPE_PROCESSED, &log_tags_list[id].counter);
      else
        stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &log_tags_list[id].counter);
    }

  stats_unlock();
}

void
log_tags_global_init(void)
{
  /* Necessary only in case of reinitialized tags */
  g_static_mutex_lock(&log_tags_lock);

  log_tags_hash = g_hash_table_new(g_str_hash, g_str_equal);

  log_tags_list_size = 4;
  log_tags_num = 0;

  log_tags_list = g_new0(LogTag, log_tags_list_size);

  g_static_mutex_unlock(&log_tags_lock);
  register_application_hook(AH_CONFIG_CHANGED, (ApplicationHookFunc) log_tags_reinit_stats, NULL);
}

void
log_tags_global_deinit(void)
{
  gint i;

  g_static_mutex_lock(&log_tags_lock);

  g_hash_table_destroy(log_tags_hash);

  stats_lock();
  StatsClusterKey sc_key;
  for (i = 0; i < log_tags_num; i++)
    {
      stats_cluster_logpipe_key_set(&sc_key, SCS_TAG, log_tags_list[i].name, NULL );
      stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &log_tags_list[i].counter);
      g_free(log_tags_list[i].name);
    }
  stats_unlock();

  log_tags_num = 0;
  g_free(log_tags_list);
  log_tags_list = NULL;
  log_tags_hash = NULL;

  g_static_mutex_unlock(&log_tags_lock);
}
