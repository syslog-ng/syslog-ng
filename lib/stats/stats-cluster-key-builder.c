/*
 * Copyright (c) 2023 Attila Szakacs
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

#include "stats/stats-cluster-key-builder.h"
#include "stats/stats-cluster-single.h"
#include "stats/stats-cluster-logpipe.h"

#include <string.h>
#include <stdio.h>

struct _StatsClusterKeyBuilder
{
  gchar *name;
  gchar *name_prefix;
  gchar *name_suffix;
  GArray *labels;
  GArray *legacy_labels;
  StatsClusterUnit unit;
  StatsClusterFrameOfReference frame_of_reference;

  struct
  {
    gboolean set;
    guint16 component;
    gchar *id;
    gchar *instance;
    gchar *name;
  } legacy;
};

static void
_label_free(StatsClusterLabel *label)
{
  g_free((gchar *) label->name);
  g_free((gchar *) label->value);
}

static gboolean
_has_legacy_labels(const StatsClusterKeyBuilder *self)
{
  return self->legacy_labels && self->legacy_labels->len != 0;
}

StatsClusterKeyBuilder *
stats_cluster_key_builder_new(void)
{
  StatsClusterKeyBuilder *self = g_new0(StatsClusterKeyBuilder, 1);

  self->labels = g_array_sized_new(FALSE, FALSE, sizeof(StatsClusterLabel), 4);
  g_array_set_clear_func(self->labels, (GDestroyNotify) _label_free);

  return self;
}

StatsClusterKeyBuilder *
stats_cluster_key_builder_clone(const StatsClusterKeyBuilder *self)
{
  StatsClusterKeyBuilder *cloned = stats_cluster_key_builder_new();

  stats_cluster_key_builder_set_name(cloned, self->name);
  stats_cluster_key_builder_set_name_prefix(cloned, self->name_prefix);
  stats_cluster_key_builder_set_name_suffix(cloned, self->name_suffix);
  for (guint i = 0; i < self->labels->len; i++)
    {
      StatsClusterLabel *label = &g_array_index(self->labels, StatsClusterLabel, i);
      stats_cluster_key_builder_add_label(cloned, stats_cluster_label(label->name, label->value));
    }
  stats_cluster_key_builder_set_unit(cloned, self->unit);
  stats_cluster_key_builder_set_frame_of_reference(cloned, self->frame_of_reference);
  stats_cluster_key_builder_set_legacy_alias(cloned, self->legacy.component, self->legacy.id, self->legacy.instance);
  stats_cluster_key_builder_set_legacy_alias_name(cloned, self->legacy.name);
  cloned->legacy.set = self->legacy.set;

  if (!_has_legacy_labels(self))
    return cloned;

  for (guint i = 0; i < self->legacy_labels->len; i++)
    {
      StatsClusterLabel *label = &g_array_index(self->legacy_labels, StatsClusterLabel, i);
      stats_cluster_key_builder_add_legacy_label(cloned, stats_cluster_label(label->name, label->value));
    }

  return cloned;
}

void
stats_cluster_key_builder_free(StatsClusterKeyBuilder *self)
{
  stats_cluster_key_builder_reset(self);
  g_array_free(self->labels, TRUE);

  if (self->legacy_labels)
    g_array_free(self->legacy_labels, TRUE);

  g_free(self);
}

void
stats_cluster_key_builder_set_name(StatsClusterKeyBuilder *self, const gchar *name)
{
  g_free(self->name);
  self->name = g_strdup(name);
}

void
stats_cluster_key_builder_set_name_prefix(StatsClusterKeyBuilder *self, const gchar *name_prefix)
{
  g_free(self->name_prefix);
  self->name_prefix = g_strdup(name_prefix);
}

void
stats_cluster_key_builder_set_name_suffix(StatsClusterKeyBuilder *self, const gchar *name_suffix)
{
  g_free(self->name_suffix);
  self->name_suffix = g_strdup(name_suffix);
}

void
stats_cluster_key_builder_add_label(StatsClusterKeyBuilder *self, const StatsClusterLabel label)
{
  StatsClusterLabel own_label = stats_cluster_label(g_strdup(label.name), g_strdup(label.value));
  self->labels = g_array_append_vals(self->labels, &own_label, 1);
}

void
stats_cluster_key_builder_set_unit(StatsClusterKeyBuilder *self, StatsClusterUnit unit)
{
  self->unit = unit;
}

void
stats_cluster_key_builder_set_frame_of_reference(StatsClusterKeyBuilder *self,
                                                 StatsClusterFrameOfReference frame_of_reference)
{
  self->frame_of_reference = frame_of_reference;
}

void
stats_cluster_key_builder_set_legacy_alias(StatsClusterKeyBuilder *self, guint16 component, const gchar *id,
                                           const gchar *instance)
{
  self->legacy.set = TRUE;
  self->legacy.component = component;

  g_free(self->legacy.id);
  self->legacy.id = g_strdup(id);

  g_free(self->legacy.instance);
  self->legacy.instance = g_strdup(instance);
}

void
stats_cluster_key_builder_set_legacy_alias_name(StatsClusterKeyBuilder *self, const gchar *name)
{
  self->legacy.set = TRUE;

  g_free(self->legacy.name);
  self->legacy.name = g_strdup(name);
}

void
stats_cluster_key_builder_reset(StatsClusterKeyBuilder *self)
{
  stats_cluster_key_builder_set_name(self, NULL);
  stats_cluster_key_builder_set_name_prefix(self, NULL);
  stats_cluster_key_builder_set_name_suffix(self, NULL);

  g_array_remove_range(self->labels, 0, self->labels->len);

  if (self->legacy_labels)
    g_array_remove_range(self->legacy_labels, 0, self->legacy_labels->len);

  stats_cluster_key_builder_set_legacy_alias(self, 0, NULL, NULL);
  stats_cluster_key_builder_set_legacy_alias_name(self, NULL);
  self->legacy.set = FALSE;

  self->unit = SCU_NONE;
  self->frame_of_reference = SCFOR_ABSOLUTE;
}

static gint
_labels_sort(const StatsClusterLabel *a, const StatsClusterLabel *b)
{
  return strcmp(a->name, b->name);
}

static gchar *
_format_name(const StatsClusterKeyBuilder *self)
{
  return g_strdup_printf("%s%s%s", self->name_prefix ? : "", self->name, self->name_suffix ? : "");
}

static gboolean
_has_new_style_values(const StatsClusterKeyBuilder *self)
{
  return !!self->name;
}

static gboolean
_has_legacy_values(const StatsClusterKeyBuilder *self)
{
  return self->legacy.set;
}

static GArray *
_construct_merged_labels(const StatsClusterKeyBuilder *self)
{
  GArray *labels = g_array_sized_new(FALSE, FALSE, sizeof(StatsClusterLabel),
                                     self->labels->len + self->legacy_labels->len);

  g_array_append_vals(labels, self->legacy_labels->data, self->legacy_labels->len);
  g_array_append_vals(labels, self->labels->data, self->labels->len);

  return labels;
}

StatsClusterKey *
stats_cluster_key_builder_build_single(const StatsClusterKeyBuilder *self)
{
  StatsClusterKey *sc_key = g_new0(StatsClusterKey, 1);
  StatsClusterKey temp_key;
  gchar *name = NULL;
  GArray *merged_labels = NULL;

  if (_has_new_style_values(self))
    {
      name = _format_name(self);
      g_array_sort(self->labels, (GCompareFunc) _labels_sort);

      if (_has_legacy_labels(self))
        {
          merged_labels = _construct_merged_labels(self);
          stats_cluster_single_key_set(&temp_key, name, (StatsClusterLabel *) merged_labels->data, merged_labels->len);
        }
      else
        stats_cluster_single_key_set(&temp_key, name, (StatsClusterLabel *) self->labels->data, self->labels->len);

      stats_cluster_single_key_add_unit(&temp_key, self->unit);
      stats_cluster_single_key_add_frame_of_reference(&temp_key, self->frame_of_reference);
    }

  if (_has_legacy_values(self))
    {
      if (_has_new_style_values(self))
        {
          if (self->legacy.name)
            stats_cluster_single_key_add_legacy_alias_with_name(&temp_key, self->legacy.component, self->legacy.id,
                                                                self->legacy.instance, self->legacy.name);
          else
            stats_cluster_single_key_add_legacy_alias(&temp_key, self->legacy.component, self->legacy.id,
                                                      self->legacy.instance);
        }
      else
        {
          if (self->legacy.name)
            stats_cluster_single_key_legacy_set_with_name(&temp_key, self->legacy.component, self->legacy.id,
                                                          self->legacy.instance, self->legacy.name);
          else
            stats_cluster_single_key_legacy_set(&temp_key, self->legacy.component, self->legacy.id,
                                                self->legacy.instance);
        }
    }

  stats_cluster_key_clone(sc_key, &temp_key);

  if (merged_labels)
    g_array_free(merged_labels, TRUE);

  g_free(name);

  return sc_key;
}

StatsClusterKey *
stats_cluster_key_builder_build_logpipe(const StatsClusterKeyBuilder *self)
{
  StatsClusterKey *sc_key = g_new0(StatsClusterKey, 1);
  StatsClusterKey temp_key;
  gchar *name = NULL;
  GArray *merged_labels = NULL;

  if (_has_new_style_values(self))
    {
      name = _format_name(self);
      g_array_sort(self->labels, (GCompareFunc) _labels_sort);

      if (_has_legacy_labels(self))
        {
          merged_labels = _construct_merged_labels(self);
          stats_cluster_logpipe_key_set(&temp_key, name, (StatsClusterLabel *) merged_labels->data, merged_labels->len);
        }
      else
        stats_cluster_logpipe_key_set(&temp_key, name, (StatsClusterLabel *) self->labels->data, self->labels->len);
    }

  if (_has_legacy_values(self))
    {
      g_assert(!self->legacy.name);
      if (_has_new_style_values(self))
        stats_cluster_logpipe_key_add_legacy_alias(&temp_key, self->legacy.component, self->legacy.id,
                                                   self->legacy.instance);
      else
        stats_cluster_logpipe_key_legacy_set(&temp_key, self->legacy.component, self->legacy.id,
                                             self->legacy.instance);
    }

  stats_cluster_key_clone(sc_key, &temp_key);

  if (merged_labels)
    g_array_free(merged_labels, TRUE);

  g_free(name);

  return sc_key;
}

void
stats_cluster_key_builder_add_legacy_label(StatsClusterKeyBuilder *self, const StatsClusterLabel label)
{
  if (!self->legacy_labels)
    {
      self->legacy_labels = g_array_sized_new(FALSE, FALSE, sizeof(StatsClusterLabel), 4);
      g_array_set_clear_func(self->legacy_labels, (GDestroyNotify) _label_free);
    }

  StatsClusterLabel own_label = stats_cluster_label(g_strdup(label.name), g_strdup(label.value));
  self->legacy_labels = g_array_append_vals(self->legacy_labels, &own_label, 1);
}

void
stats_cluster_key_builder_clear_legacy_labels(StatsClusterKeyBuilder *self)
{
  if (self->legacy_labels)
    g_array_remove_range(self->legacy_labels, 0, self->legacy_labels->len);
}

const gchar *
stats_cluster_key_builder_format_legacy_stats_instance(StatsClusterKeyBuilder *self, gchar *buf, gsize buf_size)
{
  if (!_has_legacy_labels(self))
    {
      buf[0] = '\0';
      return buf;
    }

  gboolean comma_needed = FALSE;
  gsize pos = 0;

  for (guint i = 0; i < self->legacy_labels->len; ++i)
    {
      StatsClusterLabel *l = &g_array_index(self->legacy_labels, StatsClusterLabel, i);
      pos += g_snprintf(buf + pos, buf_size - pos, "%s%s", comma_needed ? "," : "", l->value);
      pos = MIN(buf_size, pos);

      if (i == 0)
        comma_needed = TRUE;
    }

  return buf;
}
