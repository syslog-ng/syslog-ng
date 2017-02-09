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


#include "apphook.h"
#include "stats/stats-query-commands.h"
#include "stats/stats-cluster.h"
#include "stats/stats-counter.h"
#include "stats/stats-registry.h"
#include "stats/stats-query.h"
#include "syslog-ng.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>


typedef struct _CounterHashContent
{
  const gint component;
  const gchar *id;
  const gchar *instance;
  const gint type;
} CounterHashContent;

typedef struct _QueryTestCase
{
  const gchar *pattern;
  const gchar *expected;
} QueryTestCase;


static void
_initialize_counter_hash()
{
  size_t i, n;
  const CounterHashContent counters[] =
  {
    {SCS_GLOBAL, "guba.gumi.diszno", "frozen", SC_TYPE_SUPPRESSED},
    {SCS_CENTER, "guba.polo", "frozen", SC_TYPE_SUPPRESSED},
    {SCS_TCP | SCS_SOURCE, "guba.frizbi", "left", SC_TYPE_STORED},
    {SCS_FILE | SCS_SOURCE, "guba", "processed", SC_TYPE_PROCESSED},
    {SCS_PIPE | SCS_SOURCE, "guba.gumi.diszno", "frozen", SC_TYPE_SUPPRESSED},
    {SCS_TCP | SCS_DESTINATION, "guba.labda", "received", SC_TYPE_DROPPED},
  };

  stats_init();
  stats_lock();

  n = sizeof(counters) / sizeof(counters[0]);
  for (i = 0; i < n; i++)
    {
      StatsCounterItem *item = NULL;
      stats_register_counter(0, counters[i].component, counters[i].id, counters[i].instance, counters[i].type, &item);
    }

  stats_unlock();
}

TestSuite(cluster_query_key, .init = app_startup, .fini = app_shutdown);

Test(cluster_query_key, test_global_key)
{
  const gchar *expected_key = "dst.file.d_file.instance";
  StatsCluster *sc = stats_cluster_new(SCS_DESTINATION|SCS_FILE, "d_file", "instance");
  cr_assert_str_eq(sc->query_key, expected_key,
                   "generated query key(%s) does not match to the expected key(%s)",
                   sc->query_key, expected_key);
}

ParameterizedTestParameters(stats_query, test_stats_query_list)
{
  static QueryTestCase test_cases[] =
  {
    {
      NULL, "global.guba.gumi.diszno.frozen.suppressed\n"
      "src.pipe.guba.gumi.diszno.frozen.suppressed\n"
      "center.guba.polo.frozen.suppressed\n"
      "src.tcp.guba.frizbi.left.stored\n"
      "dst.tcp.guba.labda.received.dropped\n"
      "src.file.guba.processed.processed\n"
    },
    {
      "*.*", "global.guba.gumi.diszno.frozen.suppressed\n"
      "src.pipe.guba.gumi.diszno.frozen.suppressed\n"
      "center.guba.polo.frozen.suppressed\n"
      "src.tcp.guba.frizbi.left.stored\n"
      "dst.tcp.guba.labda.received.dropped\n"
      "src.file.guba.processed.processed\n"
    },
    {"center.*.*", "center.guba.polo.frozen.suppressed\n"},
    {"cent*", "center.guba.polo.frozen.suppressed\n"},
    {"src.pipe.guba.gumi.diszno.*.*", "src.pipe.guba.gumi.diszno.frozen.suppressed\n"},
    {"src.pipe.guba.gumi.*.*", "src.pipe.guba.gumi.diszno.frozen.suppressed\n"},
    {"src.pipe.guba.*.*", "src.pipe.guba.gumi.diszno.frozen.suppressed\n"},
    {"src.pipe.*.*", "src.pipe.guba.gumi.diszno.frozen.suppressed\n"},
    {
      "*.tcp.guba.*.*", "src.tcp.guba.frizbi.left.stored\n"
      "dst.tcp.guba.labda.received.dropped\n"
    },
    {
      "*.guba.*i.*.*", "global.guba.gumi.diszno.frozen.suppressed\n"
      "src.pipe.guba.gumi.diszno.frozen.suppressed\n"
      "src.tcp.guba.frizbi.left.stored\n"
    },
    {
      "*.guba.gum?.*.*", "global.guba.gumi.diszno.frozen.suppressed\n"
      "src.pipe.guba.gumi.diszno.frozen.suppressed\n"
    },
    {
      "src.*.*", "src.pipe.guba.gumi.diszno.frozen.suppressed\n"
      "src.tcp.guba.frizbi.left.stored\n"
      "src.file.guba.processed.processed\n"
    },
    {"dst.*.*", "dst.tcp.guba.labda.received.dropped\n"},
    {"dst.*.*.*", "dst.tcp.guba.labda.received.dropped\n"},
    {"dst.*.*.*.*", "dst.tcp.guba.labda.received.dropped\n"},
    {"src.java.*.*", ""},
    {"src.ja*.*.*", ""},
  };

  return cr_make_param_array(QueryTestCase, test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
}

ParameterizedTest(QueryTestCase *test_cases, stats_query, test_stats_query_list)
{
  GString *result = NULL;

  _initialize_counter_hash();

  result = stats_query_list(test_cases->pattern);
  cr_assert_str_eq(result->str, test_cases->expected,
                   "Pattern: '%s'; expected key: '%s';, got: '%s';", test_cases->pattern, test_cases->expected, result->str);
}

ParameterizedTestParameters(stats_query, test_stats_query_get)
{
  static QueryTestCase test_cases[] =
  {
    {
      "*.*", "global.guba.gumi.diszno.frozen.suppressed: 0\n"
      "src.pipe.guba.gumi.diszno.frozen.suppressed: 0\n"
      "center.guba.polo.frozen.suppressed: 0\n"
      "src.tcp.guba.frizbi.left.stored: 0\n"
      "dst.tcp.guba.labda.received.dropped: 0\n"
      "src.file.guba.processed.processed: 0\n"
    },
    {"center.*.*", "center.guba.polo.frozen.suppressed: 0\n"},
    {"cent*", "center.guba.polo.frozen.suppressed: 0\n"},
    {"src.pipe.guba.gumi.diszno.*.*", "src.pipe.guba.gumi.diszno.frozen.suppressed: 0\n"},
    {"src.pipe.guba.gumi.*.*", "src.pipe.guba.gumi.diszno.frozen.suppressed: 0\n"},
    {"src.pipe.guba.*.*", "src.pipe.guba.gumi.diszno.frozen.suppressed: 0\n"},
    {"src.pipe.*.*", "src.pipe.guba.gumi.diszno.frozen.suppressed: 0\n"},
    {
      "*.tcp.guba.*.*", "src.tcp.guba.frizbi.left.stored: 0\n"
      "dst.tcp.guba.labda.received.dropped: 0\n"
    },
    {
      "*.guba.*i.*.*", "global.guba.gumi.diszno.frozen.suppressed: 0\n"
      "src.pipe.guba.gumi.diszno.frozen.suppressed: 0\n"
      "src.tcp.guba.frizbi.left.stored: 0\n"
    },
    {
      "*.guba.gum?.*.*", "global.guba.gumi.diszno.frozen.suppressed: 0\n"
      "src.pipe.guba.gumi.diszno.frozen.suppressed: 0\n"
    },
    {
      "src.*.*", "src.pipe.guba.gumi.diszno.frozen.suppressed: 0\n"
      "src.tcp.guba.frizbi.left.stored: 0\n"
      "src.file.guba.processed.processed: 0\n"
    },
    {"dst.*.*", "dst.tcp.guba.labda.received.dropped: 0\n"},
    {"dst.*.*.*", "dst.tcp.guba.labda.received.dropped: 0\n"},
    {"dst.*.*.*.*", "dst.tcp.guba.labda.received.dropped: 0\n"},
    {"src.java.*.*", ""},
    {"src.ja*.*.*", ""},
  };

  return cr_make_param_array(QueryTestCase, test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
}

ParameterizedTest(QueryTestCase *test_cases, stats_query, test_stats_query_get)
{
  GString *result = NULL;

  _initialize_counter_hash();

  result = stats_query_get(test_cases->pattern);
  cr_assert_str_eq(result->str, test_cases->expected,
                   "Pattern: '%s'; expected key and value: '%s';, got: '%s';", test_cases->pattern, test_cases->expected, result->str);
}

ParameterizedTestParameters(stats_query, test_stats_query_get_sum)
{
  static QueryTestCase test_cases[] =
  {
    {"*.*", "0\n"},
    {"center.*.*", "0\n"},
    {"cent*", "0\n"},
    {"src.pipe.guba.gumi.diszno.*.*", "0\n"},
    {"src.pipe.guba.gumi.*.*", "0\n"},
    {"src.pipe.guba.*.*", "0\n"},
    {"src.pipe.*.*", "0\n"},
    {"*.tcp.guba.*.*", "0\n"},
    {"*.guba.*i.*.*", "0\n"},
    {"*.guba.gum?.*.*", "0\n"},
    {"src.*.*", "0\n"},
    {"dst.*.*", "0\n"},
    {"dst.*.*.*", "0\n"},
    {"dst.*.*.*.*", "0\n"},
    {"src.java.*.*", ""},
    {"src.ja*.*.*", ""},
  };

  return cr_make_param_array(QueryTestCase, test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
}

ParameterizedTest(QueryTestCase *test_cases, stats_query, test_stats_query_get_sum)
{
  GString *result = NULL;

  _initialize_counter_hash();

  result = stats_query_get_sum(test_cases->pattern);
  cr_assert_str_eq(result->str, test_cases->expected,
                   "Pattern: '%s'; expected number: '%s';, got: '%s';", test_cases->pattern, test_cases->expected, result->str);
}
