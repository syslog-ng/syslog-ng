/*
 * Copyright (c) 2017 Balabit
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
#include "mainloop.h"
#include "scratch-buffers.h"
#include "stats/stats-registry.h"
#include <criterion/criterion.h>
#include <iv.h>

#define ITERATIONS 10
#define DEFAULT_ALLOC_SIZE 256L

static void
_do_something_with_a_gstring(GString *str)
{
  g_string_assign(str, "foobar");
  g_string_append(str, "len");
  g_string_truncate(str, 0);
}

Test(scratch_buffers, alloc_returns_a_gstring_instance)
{
  GString *str = scratch_buffers_alloc();

  cr_assert_not_null(str);
  _do_something_with_a_gstring(str);
  cr_assert_eq(scratch_buffers_get_local_usage_count(), 1);
}

Test(scratch_buffers, reclaim_allocations_drops_all_allocs)
{
  for (gint i = 0; i < ITERATIONS; i++)
    {
      GString *str = scratch_buffers_alloc();
      cr_assert_not_null(str);
      _do_something_with_a_gstring(str);
    }
  cr_assert_eq(scratch_buffers_get_local_usage_count(), ITERATIONS);
  scratch_buffers_reclaim_allocations();
  cr_assert_eq(scratch_buffers_get_local_usage_count(), 0);
}

Test(scratch_buffers, reclaim_marked_allocs)
{
  ScratchBuffersMarker marker;
  GString *str;

  str = scratch_buffers_alloc();
  cr_assert_not_null(str);
  _do_something_with_a_gstring(str);

  scratch_buffers_mark(&marker);

  for (gint i = 0; i < ITERATIONS; i++)
    {
      str = scratch_buffers_alloc();
      cr_assert_not_null(str);
      _do_something_with_a_gstring(str);
    }
  cr_assert_eq(scratch_buffers_get_local_usage_count(), ITERATIONS + 1);
  scratch_buffers_reclaim_marked(marker);
  cr_assert_eq(scratch_buffers_get_local_usage_count(), 1);
}

Test(scratch_buffers, reclaim_up_to_a_marked_alloc)
{
  ScratchBuffersMarker marker;
  GString *str;

  str = scratch_buffers_alloc();
  cr_assert_not_null(str);
  _do_something_with_a_gstring(str);

  str = scratch_buffers_alloc_and_mark(&marker);
  cr_assert_not_null(str);
  _do_something_with_a_gstring(str);

  for (gint i = 0; i < ITERATIONS; i++)
    {
      str = scratch_buffers_alloc();
      cr_assert_not_null(str);
      _do_something_with_a_gstring(str);
    }
  cr_assert_eq(scratch_buffers_get_local_usage_count(), ITERATIONS + 2);
  scratch_buffers_reclaim_marked(marker);
  cr_assert_eq(scratch_buffers_get_local_usage_count(), 1);
}

Test(scratch_buffers, local_usage_metrics_measure_allocs)
{
  GString *str;

  str = scratch_buffers_alloc();
  cr_assert_not_null(str);
  _do_something_with_a_gstring(str);

  cr_assert_eq(scratch_buffers_get_local_usage_count(), 1);
  cr_assert_eq(scratch_buffers_get_local_allocation_bytes(), DEFAULT_ALLOC_SIZE);

  str = scratch_buffers_alloc();
  cr_assert_not_null(str);
  _do_something_with_a_gstring(str);
  cr_assert_eq(scratch_buffers_get_local_usage_count(), 2);
  cr_assert_eq(scratch_buffers_get_local_allocation_bytes(), 2*DEFAULT_ALLOC_SIZE);
}

/* not published via the header */
extern StatsCounterItem *stats_scratch_buffers_count;
extern StatsCounterItem *stats_scratch_buffers_bytes;

Test(scratch_buffers, stats_counters_are_updated)
{
  GString *str;

  for (gint count = 1; count <= ITERATIONS; count++)
    {
      str = scratch_buffers_alloc();
      cr_assert_not_null(str);

      /* check through accessor functions */
      cr_assert(scratch_buffers_get_local_usage_count() == count,
                "get_local_usage_count() not returning proper value, value=%d, expected=%d",
                scratch_buffers_get_local_usage_count(), count);

      cr_assert(scratch_buffers_get_local_allocation_bytes() == count * DEFAULT_ALLOC_SIZE,
                "get_local_allocation_bytes() not returning proper value, value=%ld, expected=%ld",
                scratch_buffers_get_local_allocation_bytes(), count * DEFAULT_ALLOC_SIZE);

      /* check through metrics */
      cr_assert(stats_counter_get(stats_scratch_buffers_count) == count,
                "Statistic scratch_buffers_count is not updated properly, value=%d, expected=%d",
                (gint) stats_counter_get(stats_scratch_buffers_count), count);

      /* check if byte counter is updated */
      scratch_buffers_update_stats();
      cr_assert_eq(stats_counter_get(stats_scratch_buffers_bytes), count * DEFAULT_ALLOC_SIZE);
    }
}

static void
setup(void)
{
  main_loop_thread_resource_init();
  g_thread_init(NULL);
  stats_init();
  scratch_buffers_global_init();
  scratch_buffers_allocator_init();
  scratch_buffers_register_stats();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  scratch_buffers_unregister_stats();
  scratch_buffers_allocator_deinit();
  scratch_buffers_global_deinit();
  stats_destroy();
}

TestSuite(scratch_buffers, .init = setup, .fini = teardown);
