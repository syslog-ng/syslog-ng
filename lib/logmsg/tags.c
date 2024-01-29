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
#include "stats/stats-cluster-single.h"
#include "apphook.h"

typedef struct _LogTag
{
  LogTagId id;
  gchar *name;
  StatsCounterItem *counter;
} LogTag;

static GArray *log_tags;
static GHashTable *log_tags_hash = NULL;
static GMutex log_tags_lock;

static guint
_register_tag(const gchar *name, guint id)
{
  LogTag new_tag =
  {
    .id = id,
    .name = g_strdup(name),
    0,
  };

  /* NOTE: stats-level may not be set for calls that happen during
   * config file parsing, those get fixed up by
   * log_tags_reinit_stats() below */

  stats_lock();
  StatsClusterKey sc_key;
  StatsClusterLabel labels[] = { stats_cluster_label("id", name) };
  stats_cluster_single_key_set(&sc_key, "tagged_events_total", labels, G_N_ELEMENTS(labels));
  stats_cluster_single_key_add_legacy_alias_with_name(&sc_key, SCS_TAG, name, NULL, "processed");
  stats_register_counter(3, &sc_key, SC_TYPE_SINGLE_VALUE, &new_tag.counter);
  stats_unlock();

  if (id >= log_tags->len)
    g_array_set_size(log_tags, id + 1);

  LogTag *elem = &g_array_index(log_tags, LogTag, id);

  g_assert(elem->id == 0);
  *elem = new_tag;

  g_hash_table_insert(log_tags_hash, new_tag.name, GUINT_TO_POINTER(id + 1));
  return id;
}

static guint
_register_new_tag(const gchar *name)
{
  guint id = log_tags->len;
  return _register_tag(name, id);
}

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
  guint id = 0;

  g_assert(log_tags_hash != NULL);

  g_mutex_lock(&log_tags_lock);

  gpointer key = g_hash_table_lookup(log_tags_hash, name);
  if (!key)
    {
      if (log_tags->len < LOG_TAGS_MAX - 1)
        {
          id = _register_new_tag(name);
        }
      else
        id = 0;
    }
  else
    {
      id = GPOINTER_TO_UINT(key) - 1;
    }

  g_mutex_unlock(&log_tags_lock);

  return id;
}

void
log_tags_register_predefined_tag(const gchar *name, LogTagId id)
{
  g_mutex_lock(&log_tags_lock);

  gpointer key = g_hash_table_lookup(log_tags_hash, name);
  g_assert(key == NULL);

  LogTagId rid = _register_tag(name, id);
  g_assert(rid == id);
  g_mutex_unlock(&log_tags_lock);
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

  g_mutex_lock(&log_tags_lock);

  if (id < log_tags->len)
    name = g_array_index(log_tags, LogTag, id).name;

  g_mutex_unlock(&log_tags_lock);

  return name;
}

void
log_tags_inc_counter(LogTagId id)
{
  g_mutex_lock(&log_tags_lock);

  if (id < log_tags->len)
    stats_counter_inc(g_array_index(log_tags, LogTag, id).counter);

  g_mutex_unlock(&log_tags_lock);
}

void
log_tags_dec_counter(LogTagId id)
{
  /* Reader lock because the log_tag_list is not written */
  g_mutex_lock(&log_tags_lock);

  if (id < log_tags->len)
    stats_counter_dec(g_array_index(log_tags, LogTag, id).counter);

  g_mutex_unlock(&log_tags_lock);
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

  g_mutex_lock(&log_tags_lock);
  stats_lock();

  for (id = 0; id < log_tags->len; id++)
    {
      LogTag *elem = &g_array_index(log_tags, LogTag, id);

      StatsClusterKey sc_key;
      StatsClusterLabel labels[] = { stats_cluster_label("id", elem->name) };
      stats_cluster_single_key_set(&sc_key, "tagged_events_total", labels, G_N_ELEMENTS(labels));
      stats_cluster_single_key_add_legacy_alias_with_name(&sc_key, SCS_TAG, elem->name, NULL, "processed");

      if (stats_check_level(3))
        stats_register_counter(3, &sc_key, SC_TYPE_SINGLE_VALUE, &elem->counter);
      else
        stats_unregister_counter(&sc_key, SC_TYPE_SINGLE_VALUE, &elem->counter);
    }

  stats_unlock();
  g_mutex_unlock(&log_tags_lock);
}

void
log_tags_global_init(void)
{
  log_tags_hash = g_hash_table_new(g_str_hash, g_str_equal);
  log_tags = g_array_new(FALSE, TRUE, sizeof(LogTag));

  register_application_hook(AH_CONFIG_CHANGED, (ApplicationHookFunc) log_tags_reinit_stats, NULL, AHM_RUN_REPEAT);
}

void
log_tags_global_deinit(void)
{
  g_hash_table_destroy(log_tags_hash);

  stats_lock();
  StatsClusterKey sc_key;
  for (guint id = 0; id < log_tags->len; id++)
    {
      LogTag *elem = &g_array_index(log_tags, LogTag, id);

      StatsClusterLabel labels[] = { stats_cluster_label("id", elem->name) };
      stats_cluster_single_key_set(&sc_key, "tagged_events_total", labels, G_N_ELEMENTS(labels));
      stats_cluster_single_key_add_legacy_alias_with_name(&sc_key, SCS_TAG, elem->name, NULL, "processed");
      stats_unregister_counter(&sc_key, SC_TYPE_SINGLE_VALUE, &elem->counter);
      g_free(elem->name);
    }
  stats_unlock();
  g_array_free(log_tags, TRUE);
}
