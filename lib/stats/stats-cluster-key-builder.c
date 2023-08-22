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

#define LEGACY_COMPONENT_UNSET (G_MAXUINT16)

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

  self->legacy.component = LEGACY_COMPONENT_UNSET;

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

StatsClusterKeyBuilder *
stats_cluster_key_builder_new(void)
{
  StatsClusterKeyBuilder *self = g_new0(StatsClusterKeyBuilder, 1);

  stats_cluster_key_builder_push(self);

  return self;
}

void
stats_cluster_key_builder_push(StatsClusterKeyBuilder *self)
{
  self->options_stack = g_list_append(self->options_stack, _options_new());
}

void
stats_cluster_key_builder_pop(StatsClusterKeyBuilder *self)
{
  GList *last_link = g_list_last(self->options_stack);
  BuilderOptions *options = (BuilderOptions *) last_link->data;

  self->options_stack = g_list_delete_link(self->options_stack, last_link);
  _options_free(options);
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

  g_free(options->legacy.name);
  options->legacy.name = g_strdup(name);
}

static gint
_labels_sort(const StatsClusterLabel *a, const StatsClusterLabel *b)
{
  return strcmp(a->name, b->name);
}

static gchar *
_format_name(const StatsClusterKeyBuilder *self)
{
  const gchar *name_prefix = NULL;
  const gchar *name = NULL;
  const gchar *name_suffix = NULL;

  for (const GList *link = g_list_last(self->options_stack); link; link = link->prev)
    {
      const BuilderOptions *options = (const BuilderOptions *) link->data;

      if (!name_prefix && options->name_prefix)
        name_prefix = options->name_prefix;

      if (!name && options->name)
        name = options->name;

      if (!name_suffix && options->name_suffix)
        name_suffix = options->name_suffix;

      if (name_prefix && name && name_suffix)
        break;
    }

  return g_strdup_printf("%s%s%s", name_prefix ? : "", name ? : "", name_suffix ? : "");
}

static gboolean
_has_new_style_values(const StatsClusterKeyBuilder *self)
{
  for (const GList *link = g_list_last(self->options_stack); link; link = link->prev)
    {
      const BuilderOptions *options = (const BuilderOptions *) link->data;

      if (options->name)
        return strlen(options->name) > 0;
    }

  return FALSE;
}

static gboolean
_has_legacy_values(const StatsClusterKeyBuilder *self)
{
  const gchar *name = NULL;
  const gchar *id = NULL;

  for (const GList *link = g_list_last(self->options_stack); link; link = link->prev)
    {
      const BuilderOptions *options = (const BuilderOptions *) link->data;

      if (!name && options->legacy.name)
        name = options->legacy.name;

      if (!id && options->legacy.id)
        id = options->legacy.id;

      if (name && id)
        break;
    }

  return (name && strlen(name) > 0) || (id && strlen(id) > 0);
}

static GArray *
_construct_merged_labels(const StatsClusterKeyBuilder *self)
{
  GArray *merged_labels = g_array_sized_new(FALSE, FALSE, sizeof(StatsClusterLabel), 4);

  for (const GList *link = g_list_first(self->options_stack); link; link = link->next)
    {
      const BuilderOptions *options = (const BuilderOptions *) link->data;

      if (options->legacy_labels)
        merged_labels = g_array_append_vals(merged_labels, options->legacy_labels->data, options->legacy_labels->len);
    }

  GArray *merged_unsorted_labels = g_array_sized_new(FALSE, FALSE, sizeof(StatsClusterLabel), 4);
  for (const GList *link = g_list_first(self->options_stack); link; link = link->next)
    {
      const BuilderOptions *options = (const BuilderOptions *) link->data;

      if (options->labels)
        merged_unsorted_labels = g_array_append_vals(merged_unsorted_labels, options->labels->data,
                                                     options->labels->len);
    }
  g_array_sort(merged_unsorted_labels, (GCompareFunc) _labels_sort);
  merged_labels = g_array_append_vals(merged_labels, merged_unsorted_labels->data, merged_unsorted_labels->len);

  g_array_free(merged_unsorted_labels, TRUE);
  return merged_labels;
}

static GArray *
_construct_merged_legacy_labels(const StatsClusterKeyBuilder *self)
{
  GArray *merged_labels = g_array_sized_new(FALSE, FALSE, sizeof(StatsClusterLabel), 4);

  for (const GList *link = g_list_first(self->options_stack); link; link = link->next)
    {
      const BuilderOptions *options = (const BuilderOptions *) link->data;

      if (options->legacy_labels)
        merged_labels = g_array_append_vals(merged_labels, options->legacy_labels->data, options->legacy_labels->len);
    }

  return merged_labels;
}

static void
_get_legacy_options(const StatsClusterKeyBuilder *self, guint16 *component, const gchar **id, const gchar **instance,
                    const gchar **name)
{
  *component = LEGACY_COMPONENT_UNSET;
  *id = *instance = *name = NULL;

  for (const GList *link = g_list_last(self->options_stack); link; link = link->prev)
    {
      const BuilderOptions *options = (const BuilderOptions *) link->data;

      if (*component == LEGACY_COMPONENT_UNSET && options->legacy.component != LEGACY_COMPONENT_UNSET)
        *component = options->legacy.component;

      if (!*id && options->legacy.id)
        *id = options->legacy.id;

      if (!*instance && options->legacy.instance)
        *instance = options->legacy.instance;

      if (!*name && options->legacy.name)
        *name = options->legacy.name;

      if (*component != G_MAXUINT16 && *id && *instance && *name)
        break;
    }
}

StatsClusterUnit
_get_unit(const StatsClusterKeyBuilder *self)
{
  for (const GList *link = g_list_last(self->options_stack); link; link = link->prev)
    {
      const BuilderOptions *options = (const BuilderOptions *) link->data;

      if (options->unit != SCU_NONE)
        return options->unit;
    }

  return SCU_NONE;
}

StatsClusterFrameOfReference
_get_frame_of_reference(const StatsClusterKeyBuilder *self)
{
  for (const GList *link = g_list_last(self->options_stack); link; link = link->prev)
    {
      const BuilderOptions *options = (const BuilderOptions *) link->data;

      if (options->frame_of_reference != SCFOR_NONE)
        return options->frame_of_reference;
    }

  return SCFOR_NONE;
}

StatsClusterKey *
stats_cluster_key_builder_build_single(const StatsClusterKeyBuilder *self)
{
  StatsClusterKey *sc_key = g_new0(StatsClusterKey, 1);
  StatsClusterKey temp_key;
  gchar *name = NULL;

  gboolean has_new_style_values = _has_new_style_values(self);
  gboolean has_legacy_values = _has_legacy_values(self);

  GArray *merged_labels = _construct_merged_labels(self);

  if (has_new_style_values)
    {
      name = _format_name(self);
      stats_cluster_single_key_set(&temp_key, name, (StatsClusterLabel *) merged_labels->data, merged_labels->len);
      stats_cluster_single_key_add_unit(&temp_key, _get_unit(self));
      stats_cluster_single_key_add_frame_of_reference(&temp_key, _get_frame_of_reference(self));
    }

  if (has_legacy_values)
    {
      guint16 legacy_component;
      const gchar *legacy_id, *legacy_instance, *legacy_name;
      _get_legacy_options(self, &legacy_component, &legacy_id, &legacy_instance, &legacy_name);

      if (has_new_style_values)
        {
          if (legacy_name && strlen(legacy_name) > 0)
            stats_cluster_single_key_add_legacy_alias_with_name(&temp_key, legacy_component, legacy_id, legacy_instance,
                                                                legacy_name);
          else
            stats_cluster_single_key_add_legacy_alias(&temp_key, legacy_component, legacy_id, legacy_instance);
        }
      else
        {
          if (legacy_name && strlen(legacy_name) > 0)
            stats_cluster_single_key_legacy_set_with_name(&temp_key, legacy_component, legacy_id, legacy_instance,
                                                          legacy_name);
          else
            stats_cluster_single_key_legacy_set(&temp_key, legacy_component, legacy_id, legacy_instance);
        }
    }

  stats_cluster_key_clone(sc_key, &temp_key);

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

  gboolean has_new_style_values = _has_new_style_values(self);
  gboolean has_legacy_values = _has_legacy_values(self);

  GArray *merged_labels = _construct_merged_labels(self);

  if (has_new_style_values)
    {
      name = _format_name(self);
      stats_cluster_logpipe_key_set(&temp_key, name, (StatsClusterLabel *) merged_labels->data, merged_labels->len);
    }

  if (has_legacy_values)
    {
      guint16 legacy_component;
      const gchar *legacy_id, *legacy_instance, *legacy_name;
      _get_legacy_options(self, &legacy_component, &legacy_id, &legacy_instance, &legacy_name);

      g_assert(!legacy_name || strlen(legacy_name) == 0);

      if (has_new_style_values)
        stats_cluster_logpipe_key_add_legacy_alias(&temp_key, legacy_component, legacy_id, legacy_instance);
      else
        stats_cluster_logpipe_key_legacy_set(&temp_key, legacy_component, legacy_id, legacy_instance);
    }

  stats_cluster_key_clone(sc_key, &temp_key);

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

const gchar *
stats_cluster_key_builder_format_legacy_stats_instance(const StatsClusterKeyBuilder *self, gchar *buf, gsize buf_size)
{
  GArray *merged_legacy_labels = _construct_merged_legacy_labels(self);

  if (merged_legacy_labels->len == 0)
    {
      buf[0] = '\0';
      goto exit;
    }

  gboolean comma_needed = FALSE;
  gsize pos = 0;

  for (guint i = 0; i < merged_legacy_labels->len; ++i)
    {
      StatsClusterLabel *l = &g_array_index(merged_legacy_labels, StatsClusterLabel, i);
      pos += g_snprintf(buf + pos, buf_size - pos, "%s%s", comma_needed ? "," : "", l->value);
      pos = MIN(buf_size, pos);

      if (i == 0)
        comma_needed = TRUE;
    }

exit:
  g_array_free(merged_legacy_labels, TRUE);
  return buf;
}
