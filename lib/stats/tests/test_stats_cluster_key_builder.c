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

#include <criterion/criterion.h>

#include "stats/stats-cluster-key-builder.h"
#include "stats/stats-cluster-single.h"
#include "stats/stats-cluster-logpipe.h"

typedef enum
{
  TEST_LOGPIPE,
  TEST_SINGLE,
} KeyType;

static void
_assert_built_sc_key_equals(const StatsClusterKeyBuilder *builder, KeyType type, const gchar *name,
                            StatsClusterLabel *labels, gssize labels_len)
{
  StatsClusterKey expected_sc_key;
  StatsClusterKey *built_key;

  if (type == TEST_LOGPIPE)
    {
      stats_cluster_logpipe_key_set(&expected_sc_key, name, labels, labels_len);
      built_key = stats_cluster_key_builder_build_logpipe(builder);
    }
  else if (type == TEST_SINGLE)
    {
      stats_cluster_single_key_set(&expected_sc_key, name, labels, labels_len);
      built_key = stats_cluster_key_builder_build_single(builder);
    }
  else
    {
      cr_assert_fail();
    }

  cr_assert(stats_cluster_key_equal(&expected_sc_key, built_key));
  cr_assert_eq(memcmp(&expected_sc_key.formatting, &built_key->formatting, sizeof(built_key->formatting)), 0);

  stats_cluster_key_free(built_key);
}

static void
_assert_built_sc_key_has_unit(const StatsClusterKeyBuilder *builder, KeyType type, StatsClusterUnit unit)
{
  StatsClusterKey *built_key = stats_cluster_key_builder_build_single(builder);

  if (type == TEST_SINGLE)
    cr_assert(built_key->formatting.stored_unit == unit);

  stats_cluster_key_free(built_key);
}

static void
_assert_built_sc_key_has_frame_of_reference(const StatsClusterKeyBuilder *builder, KeyType type,
                                            StatsClusterFrameOfReference frame_of_reference)
{
  StatsClusterKey *built_key = stats_cluster_key_builder_build_single(builder);

  if (type == TEST_SINGLE)
    cr_assert(built_key->formatting.frame_of_reference == frame_of_reference);

  stats_cluster_key_free(built_key);
}

static void
_assert_built_sc_key_equals_with_legacy(const StatsClusterKeyBuilder *builder, KeyType type, const gchar *name,
                                        StatsClusterLabel *labels, gssize labels_len, guint16 legacy_component,
                                        const gchar *legacy_id, const gchar *legacy_instance,
                                        const gchar *legacy_name)
{
  StatsClusterKey expected_sc_key;
  StatsClusterKey *built_key;

  if (type == TEST_LOGPIPE)
    {
      stats_cluster_logpipe_key_set(&expected_sc_key, name, labels, labels_len);
      stats_cluster_logpipe_key_add_legacy_alias(&expected_sc_key, legacy_component, legacy_id, legacy_instance);
      built_key = stats_cluster_key_builder_build_logpipe(builder);
    }
  else if (type == TEST_SINGLE)
    {
      stats_cluster_single_key_set(&expected_sc_key, name, labels, labels_len);
      if (legacy_name)
        {
          stats_cluster_single_key_add_legacy_alias_with_name(&expected_sc_key, legacy_component, legacy_id,
                                                              legacy_instance, legacy_name);
        }
      else
        {
          stats_cluster_single_key_add_legacy_alias(&expected_sc_key, legacy_component, legacy_id, legacy_instance);
        }
      built_key = stats_cluster_key_builder_build_single(builder);
    }
  else
    {
      cr_assert_fail();
    }

  cr_assert(stats_cluster_key_equal(&expected_sc_key, built_key));

  stats_cluster_key_free(built_key);
}
static void
_assert_built_sc_key_equals_with_legacy_only(const StatsClusterKeyBuilder *builder, KeyType type,
                                             guint16 legacy_component, const gchar *legacy_id,
                                             const gchar *legacy_instance)
{
  StatsClusterKey expected_sc_key;
  StatsClusterKey *built_key = NULL;

  if (type == TEST_LOGPIPE)
    {
      stats_cluster_logpipe_key_legacy_set(&expected_sc_key, legacy_component, legacy_id, legacy_instance);
      built_key = stats_cluster_key_builder_build_logpipe(builder);
    }
  else if (type == TEST_SINGLE)
    {
      stats_cluster_single_key_legacy_set(&expected_sc_key, legacy_component, legacy_id, legacy_instance);
      built_key = stats_cluster_key_builder_build_single(builder);
    }
  else
    {
      cr_assert_fail();
    }

  cr_assert(!built_key->name);
  cr_assert(stats_cluster_key_equal(&expected_sc_key, built_key));

  stats_cluster_key_free(built_key);
}

static void
_test_builder(KeyType type)
{
  StatsClusterKeyBuilder *builder = stats_cluster_key_builder_new();

  const gchar *dummy_name = "dummy_name";
  const gchar *dummy_name_2 = "dummy_name_2";
  const gchar *dummy_name_prefix = "dummy_name_prefix_";
  const gchar *dummy_name_with_prefix = "dummy_name_prefix_dummy_name";
  const gchar *dummy_name_with_foobar_prefix = "foobardummy_name";
  const gchar *dummy_name_suffix = "_dummy_name_suffix";
  const gchar *dummy_name_with_suffix = "dummy_name_dummy_name_suffix";
  const gchar *dummy_name_with_foobar_suffix = "dummy_namefoobar";

  guint16 dummy_legacy_component = 42;
  const gchar *dummy_legacy_id = "dummy_legacy_id";
  const gchar *dummy_legacy_instance = "dummy_legacy_instance";
  const gchar *dummy_legacy_name = "dummy_legacy_name";

  stats_cluster_key_builder_push(builder);
  {
    /* Name only */
    stats_cluster_key_builder_set_name(builder, dummy_name);
    StatsClusterLabel empty_labels[] = {};
    _assert_built_sc_key_equals(builder, type, dummy_name, empty_labels, G_N_ELEMENTS(empty_labels));

    /* One label */
    stats_cluster_key_builder_push(builder);
    {
      stats_cluster_key_builder_add_label(builder, stats_cluster_label("dummy-label-name-99", "dummy-label-value-99"));
      StatsClusterLabel one_label[] =
      {
        stats_cluster_label("dummy-label-name-99", "dummy-label-value-99"),
      };
      _assert_built_sc_key_equals(builder, type, dummy_name, one_label, G_N_ELEMENTS(one_label));
    }
    stats_cluster_key_builder_pop(builder);
    _assert_built_sc_key_equals(builder, type, dummy_name, empty_labels, G_N_ELEMENTS(empty_labels));

    /* Two labels, set in wrong ordering */
    stats_cluster_key_builder_push(builder);
    {
      stats_cluster_key_builder_add_label(builder, stats_cluster_label("dummy-label-name-99", "dummy-label-value-99"));
      stats_cluster_key_builder_push(builder);
      {
        stats_cluster_key_builder_add_label(builder,
                                            stats_cluster_label("dummy-label-name-00", "dummy-label-value-00"));
        StatsClusterLabel two_labels[] =
        {
          stats_cluster_label("dummy-label-name-00", "dummy-label-value-00"),
          stats_cluster_label("dummy-label-name-99", "dummy-label-value-99"),
        };
        _assert_built_sc_key_equals(builder, type, dummy_name, two_labels, G_N_ELEMENTS(two_labels));
      }
      stats_cluster_key_builder_pop(builder);
    }
    stats_cluster_key_builder_pop(builder);
    _assert_built_sc_key_equals(builder, type, dummy_name, empty_labels, G_N_ELEMENTS(empty_labels));

    /* New name */
    stats_cluster_key_builder_push(builder);
    {
      stats_cluster_key_builder_set_name(builder, dummy_name_2);
      _assert_built_sc_key_equals(builder, type, dummy_name_2, empty_labels, G_N_ELEMENTS(empty_labels));
    }
    stats_cluster_key_builder_pop(builder);

    /* Name prefix */
    stats_cluster_key_builder_push(builder);
    {
      stats_cluster_key_builder_set_name_prefix(builder, dummy_name_prefix);
      stats_cluster_key_builder_push(builder);
      {
        _assert_built_sc_key_equals(builder, type, dummy_name_with_prefix, empty_labels, G_N_ELEMENTS(empty_labels));
        stats_cluster_key_builder_set_name_prefix(builder, "foobar");
        _assert_built_sc_key_equals(builder, type, dummy_name_with_foobar_prefix, empty_labels,
                                    G_N_ELEMENTS(empty_labels));
      }
      stats_cluster_key_builder_pop(builder);
      _assert_built_sc_key_equals(builder, type, dummy_name_with_prefix, empty_labels, G_N_ELEMENTS(empty_labels));
    }
    stats_cluster_key_builder_pop(builder);

    /* Name suffix */
    stats_cluster_key_builder_push(builder);
    {
      stats_cluster_key_builder_set_name_suffix(builder, dummy_name_suffix);
      stats_cluster_key_builder_push(builder);
      {
        _assert_built_sc_key_equals(builder, type, dummy_name_with_suffix, empty_labels, G_N_ELEMENTS(empty_labels));
        stats_cluster_key_builder_set_name_suffix(builder, "foobar");
        _assert_built_sc_key_equals(builder, type, dummy_name_with_foobar_suffix, empty_labels,
                                    G_N_ELEMENTS(empty_labels));
      }
      stats_cluster_key_builder_pop(builder);
      _assert_built_sc_key_equals(builder, type, dummy_name_with_suffix, empty_labels, G_N_ELEMENTS(empty_labels));
    }
    stats_cluster_key_builder_pop(builder);

    /* Unit */
    stats_cluster_key_builder_push(builder);
    {
      stats_cluster_key_builder_set_unit(builder, SCU_NANOSECONDS);
      stats_cluster_key_builder_push(builder);
      {
        _assert_built_sc_key_has_unit(builder, type, SCU_NANOSECONDS);
        stats_cluster_key_builder_set_unit(builder, SCU_KIB);
        _assert_built_sc_key_has_unit(builder, type, SCU_KIB);
      }
      stats_cluster_key_builder_pop(builder);
      _assert_built_sc_key_has_unit(builder, type, SCU_NANOSECONDS);
    }
    stats_cluster_key_builder_pop(builder);

    /* Frame of reference */
    stats_cluster_key_builder_push(builder);
    {
      stats_cluster_key_builder_set_frame_of_reference(builder, SCFOR_RELATIVE_TO_TIME_OF_QUERY);
      stats_cluster_key_builder_push(builder);
      {
        _assert_built_sc_key_has_frame_of_reference(builder, type, SCFOR_RELATIVE_TO_TIME_OF_QUERY);
        stats_cluster_key_builder_set_frame_of_reference(builder, SCFOR_ABSOLUTE);
        _assert_built_sc_key_has_frame_of_reference(builder, type, SCFOR_ABSOLUTE);
      }
      stats_cluster_key_builder_pop(builder);
      _assert_built_sc_key_has_frame_of_reference(builder, type, SCFOR_RELATIVE_TO_TIME_OF_QUERY);
    }
    stats_cluster_key_builder_pop(builder);

    /* Legacy alias */
    stats_cluster_key_builder_push(builder);
    {
      stats_cluster_key_builder_set_legacy_alias(builder, 1337, "foobar", "foobar");
      stats_cluster_key_builder_push(builder);
      {
        _assert_built_sc_key_equals_with_legacy(builder, type, dummy_name, empty_labels,
                                                G_N_ELEMENTS(empty_labels), 1337, "foobar", "foobar", NULL);
        stats_cluster_key_builder_set_legacy_alias(builder, dummy_legacy_component, dummy_legacy_id,
                                                   dummy_legacy_instance);
        _assert_built_sc_key_equals_with_legacy(builder, type, dummy_name, empty_labels, G_N_ELEMENTS(empty_labels),
                                                dummy_legacy_component, dummy_legacy_id, dummy_legacy_instance, NULL);
      }
      stats_cluster_key_builder_pop(builder);
      _assert_built_sc_key_equals_with_legacy(builder, type, dummy_name, empty_labels,
                                              G_N_ELEMENTS(empty_labels), 1337, "foobar", "foobar", NULL);
    }
    stats_cluster_key_builder_pop(builder);

    /* Legacy alias name */
    if (type == TEST_SINGLE)
      {
        /* LOGPIPE does not support setting the legacy name */
        stats_cluster_key_builder_push(builder);
        {
          stats_cluster_key_builder_set_legacy_alias(builder, dummy_legacy_component, dummy_legacy_id,
                                                     dummy_legacy_instance);
          stats_cluster_key_builder_set_legacy_alias_name(builder, "foobar");
          stats_cluster_key_builder_push(builder);
          {
            _assert_built_sc_key_equals_with_legacy(builder, type, dummy_name, empty_labels, G_N_ELEMENTS(empty_labels),
                                                    dummy_legacy_component, dummy_legacy_id, dummy_legacy_instance,
                                                    "foobar");
            stats_cluster_key_builder_set_legacy_alias_name(builder, dummy_legacy_name);
            _assert_built_sc_key_equals_with_legacy(builder, type, dummy_name, empty_labels, G_N_ELEMENTS(empty_labels),
                                                    dummy_legacy_component, dummy_legacy_id, dummy_legacy_instance,
                                                    dummy_legacy_name);
          }
          stats_cluster_key_builder_pop(builder);
          _assert_built_sc_key_equals_with_legacy(builder, type, dummy_name, empty_labels, G_N_ELEMENTS(empty_labels),
                                                  dummy_legacy_component, dummy_legacy_id, dummy_legacy_instance,
                                                  "foobar");
        }
        stats_cluster_key_builder_pop(builder);
      }
  }
  stats_cluster_key_builder_pop(builder);

  /* Legacy only */
  stats_cluster_key_builder_push(builder);
  {
    stats_cluster_key_builder_set_legacy_alias(builder, 1337, "foobar", "foobar");
    stats_cluster_key_builder_push(builder);
    {
      _assert_built_sc_key_equals_with_legacy_only(builder, type, 1337, "foobar", "foobar");
      stats_cluster_key_builder_set_legacy_alias(builder, dummy_legacy_component, dummy_legacy_id,
                                                 dummy_legacy_instance);
      _assert_built_sc_key_equals_with_legacy_only(builder, type, dummy_legacy_component, dummy_legacy_id,
                                                   dummy_legacy_instance);
    }
    stats_cluster_key_builder_pop(builder);
    _assert_built_sc_key_equals_with_legacy_only(builder, type, 1337, "foobar", "foobar");
  }
  stats_cluster_key_builder_pop(builder);

  stats_cluster_key_builder_free(builder);
}

Test(stats_cluster_key_builder, single)
{
  _test_builder(TEST_SINGLE);
}

Test(stats_cluster_key_builder, logpipe)
{
  _test_builder(TEST_LOGPIPE);
}

TestSuite(stats_cluster_key_builder);
