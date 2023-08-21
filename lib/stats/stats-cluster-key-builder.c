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

typedef struct _BuilderOptions
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
} BuilderOptions;

struct _StatsClusterKeyBuilder
{
  GList *options_stack;
};

static void
_label_free(StatsClusterLabel *label)
{
  g_free((gchar *) label->name);
  g_free((gchar *) label->value);
}

static BuilderOptions *
_options_new(void)
{
  BuilderOptions *self = g_new0(BuilderOptions, 1);

  self->labels = g_array_sized_new(FALSE, FALSE, sizeof(StatsClusterLabel), 4);
  g_array_set_clear_func(self->labels, (GDestroyNotify) _label_free);

  return self;
}

static void
_options_free(BuilderOptions *self)
{
  g_free(self->name);
  g_free(self->name_prefix);
  g_free(self->name_suffix);

  g_free(self->legacy.id);
  g_free(self->legacy.instance);
  g_free(self->legacy.name);

  g_array_free(self->labels, TRUE);

  if (self->legacy_labels)
    g_array_free(self->legacy_labels, TRUE);

  g_free(self);
}

static gboolean
_has_legacy_labels(const BuilderOptions *self)
{
  return self->legacy_labels && self->legacy_labels->len != 0;
}

StatsClusterKeyBuilder *
stats_cluster_key_builder_new(void)
{
  StatsClusterKeyBuilder *self = g_new0(StatsClusterKeyBuilder, 1);

  self->options_stack = g_list_append(self->options_stack, _options_new());

  return self;
}

StatsClusterKeyBuilder *
stats_cluster_key_builder_clone(const StatsClusterKeyBuilder *self)
{
  /* TODO: Remove */
  return NULL;
}

void
stats_cluster_key_builder_free(StatsClusterKeyBuilder *self)
{
  g_list_free_full(self->options_stack, (GDestroyNotify) _options_free);
  g_free(self);
}

static BuilderOptions *
_get_last_options(StatsClusterKeyBuilder *self)
{
  return (BuilderOptions *) g_list_last(self->options_stack)->data;
}

static const BuilderOptions *
_get_last_options_const(const StatsClusterKeyBuilder *self)
{
  return (const BuilderOptions *) g_list_last(self->options_stack)->data;
}

void
stats_cluster_key_builder_set_name(StatsClusterKeyBuilder *self, const gchar *name)
{
  BuilderOptions *options = _get_last_options(self);

  g_free(options->name);
  options->name = g_strdup(name);
}

void
stats_cluster_key_builder_set_name_prefix(StatsClusterKeyBuilder *self, const gchar *name_prefix)
{
  BuilderOptions *options = _get_last_options(self);

  g_free(options->name_prefix);
  options->name_prefix = g_strdup(name_prefix);
}

void
stats_cluster_key_builder_set_name_suffix(StatsClusterKeyBuilder *self, const gchar *name_suffix)
{
  BuilderOptions *options = _get_last_options(self);

  g_free(options->name_suffix);
  options->name_suffix = g_strdup(name_suffix);
}

void
stats_cluster_key_builder_add_label(StatsClusterKeyBuilder *self, const StatsClusterLabel label)
{
  BuilderOptions *options = _get_last_options(self);

  StatsClusterLabel own_label = stats_cluster_label(g_strdup(label.name), g_strdup(label.value));
  options->labels = g_array_append_vals(options->labels, &own_label, 1);
}

void
stats_cluster_key_builder_set_unit(StatsClusterKeyBuilder *self, StatsClusterUnit unit)
{
  BuilderOptions *options = _get_last_options(self);

  options->unit = unit;
}

void
stats_cluster_key_builder_set_frame_of_reference(StatsClusterKeyBuilder *self,
                                                 StatsClusterFrameOfReference frame_of_reference)
{
  BuilderOptions *options = _get_last_options(self);

  options->frame_of_reference = frame_of_reference;
}

void
stats_cluster_key_builder_set_legacy_alias(StatsClusterKeyBuilder *self, guint16 component, const gchar *id,
                                           const gchar *instance)
{
  BuilderOptions *options = _get_last_options(self);

  options->legacy.set = TRUE;
  options->legacy.component = component;

  g_free(options->legacy.id);
  options->legacy.id = g_strdup(id);

  g_free(options->legacy.instance);
  options->legacy.instance = g_strdup(instance);
}

void
stats_cluster_key_builder_set_legacy_alias_name(StatsClusterKeyBuilder *self, const gchar *name)
{
  BuilderOptions *options = _get_last_options(self);

  options->legacy.set = TRUE;

  g_free(options->legacy.name);
  options->legacy.name = g_strdup(name);
}

void
stats_cluster_key_builder_reset(StatsClusterKeyBuilder *self)
{
  /* TODO: Remove */
}

static gint
_labels_sort(const StatsClusterLabel *a, const StatsClusterLabel *b)
{
  return strcmp(a->name, b->name);
}

static gchar *
_format_name(const StatsClusterKeyBuilder *self)
{
  const BuilderOptions *options = _get_last_options_const(self);

  return g_strdup_printf("%s%s%s", options->name_prefix ? : "", options->name, options->name_suffix ? : "");
}

static gboolean
_has_new_style_values(const StatsClusterKeyBuilder *self)
{
  const BuilderOptions *options = _get_last_options_const(self);

  return !!options->name;
}

static gboolean
_has_legacy_values(const StatsClusterKeyBuilder *self)
{
  const BuilderOptions *options = _get_last_options_const(self);

  return options->legacy.set;
}

static GArray *
_construct_merged_labels(const StatsClusterKeyBuilder *self)
{
  const BuilderOptions *options = _get_last_options_const(self);

  GArray *labels = g_array_sized_new(FALSE, FALSE, sizeof(StatsClusterLabel),
                                     options->labels->len + options->legacy_labels->len);

  g_array_append_vals(labels, options->legacy_labels->data, options->legacy_labels->len);
  g_array_append_vals(labels, options->labels->data, options->labels->len);

  return labels;
}

StatsClusterKey *
stats_cluster_key_builder_build_single(const StatsClusterKeyBuilder *self)
{
  StatsClusterKey *sc_key = g_new0(StatsClusterKey, 1);
  StatsClusterKey temp_key;
  gchar *name = NULL;
  GArray *merged_labels = NULL;
  const BuilderOptions *options = _get_last_options_const(self);

  if (_has_new_style_values(self))
    {
      name = _format_name(self);
      g_array_sort(options->labels, (GCompareFunc) _labels_sort);

      if (_has_legacy_labels(options))
        {
          merged_labels = _construct_merged_labels(self);
          stats_cluster_single_key_set(&temp_key, name, (StatsClusterLabel *) merged_labels->data, merged_labels->len);
        }
      else
        stats_cluster_single_key_set(&temp_key, name, (StatsClusterLabel *) options->labels->data, options->labels->len);

      stats_cluster_single_key_add_unit(&temp_key, options->unit);
      stats_cluster_single_key_add_frame_of_reference(&temp_key, options->frame_of_reference);
    }

  if (_has_legacy_values(self))
    {
      if (_has_new_style_values(self))
        {
          if (options->legacy.name)
            stats_cluster_single_key_add_legacy_alias_with_name(&temp_key, options->legacy.component, options->legacy.id,
                                                                options->legacy.instance, options->legacy.name);
          else
            stats_cluster_single_key_add_legacy_alias(&temp_key, options->legacy.component, options->legacy.id,
                                                      options->legacy.instance);
        }
      else
        {
          if (options->legacy.name)
            stats_cluster_single_key_legacy_set_with_name(&temp_key, options->legacy.component, options->legacy.id,
                                                          options->legacy.instance, options->legacy.name);
          else
            stats_cluster_single_key_legacy_set(&temp_key, options->legacy.component, options->legacy.id,
                                                options->legacy.instance);
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
  const BuilderOptions *options = _get_last_options_const(self);

  if (_has_new_style_values(self))
    {
      name = _format_name(self);
      g_array_sort(options->labels, (GCompareFunc) _labels_sort);

      if (_has_legacy_labels(options))
        {
          merged_labels = _construct_merged_labels(self);
          stats_cluster_logpipe_key_set(&temp_key, name, (StatsClusterLabel *) merged_labels->data, merged_labels->len);
        }
      else
        stats_cluster_logpipe_key_set(&temp_key, name, (StatsClusterLabel *) options->labels->data,
                                      options->labels->len);
    }

  if (_has_legacy_values(self))
    {
      g_assert(!options->legacy.name);
      if (_has_new_style_values(self))
        stats_cluster_logpipe_key_add_legacy_alias(&temp_key, options->legacy.component, options->legacy.id,
                                                   options->legacy.instance);
      else
        stats_cluster_logpipe_key_legacy_set(&temp_key, options->legacy.component, options->legacy.id,
                                             options->legacy.instance);
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
  BuilderOptions *options = _get_last_options(self);

  if (!options->legacy_labels)
    {
      options->legacy_labels = g_array_sized_new(FALSE, FALSE, sizeof(StatsClusterLabel), 4);
      g_array_set_clear_func(options->legacy_labels, (GDestroyNotify) _label_free);
    }

  StatsClusterLabel own_label = stats_cluster_label(g_strdup(label.name), g_strdup(label.value));
  options->legacy_labels = g_array_append_vals(options->legacy_labels, &own_label, 1);
}

void
stats_cluster_key_builder_clear_legacy_labels(StatsClusterKeyBuilder *self)
{
  /* TODO: Remove */
}

const gchar *
stats_cluster_key_builder_format_legacy_stats_instance(StatsClusterKeyBuilder *self, gchar *buf, gsize buf_size)
{
  BuilderOptions *options = _get_last_options(self);

  if (!_has_legacy_labels(options))
    {
      buf[0] = '\0';
      return buf;
    }

  gboolean comma_needed = FALSE;
  gsize pos = 0;

  for (guint i = 0; i < options->legacy_labels->len; ++i)
    {
      StatsClusterLabel *l = &g_array_index(options->legacy_labels, StatsClusterLabel, i);
      pos += g_snprintf(buf + pos, buf_size - pos, "%s%s", comma_needed ? "," : "", l->value);
      pos = MIN(buf_size, pos);

      if (i == 0)
        comma_needed = TRUE;
    }

  return buf;
}
