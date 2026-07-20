/*
 * Copyright (c) 2026 One Identity
 * Copyright (c) 2026 Hofi <hofione@gmail.com>
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

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include "kafka-topic-parts.h"
#include "kafka-internal.h"
#include "messages.h"
#include "logmsg/logmsg.h"

/*
 * Helpers
 *
 * Production code stores partition numbers as GINT_TO_POINTER(int32_t).
 * The test fixtures mirror that so kafka_tps_equal()'s pointer-identity
 * comparison behaves the same as in the real pipeline.
 */
static GList *
_make_partitions(gint count, ...)
{
  GList *list = NULL;
  va_list ap;
  va_start(ap, count);
  for (gint i = 0; i < count; i++)
    list = g_list_append(list, GINT_TO_POINTER(va_arg(ap, gint)));
  va_end(ap);
  return list;
}

static void
setup(void)
{
  msg_init(FALSE);
  log_tags_global_init();
  log_msg_global_init();
}

static void
teardown(void)
{
  msg_deinit();
}

TestSuite(kafka_topic_utils, .init = setup, .fini = teardown);

/* *********************
 *  Construction
 * *********************/

Test(kafka_topic_utils, tps_new_allocates_struct_and_dups_topic)
{
  gchar topic_buf[] = "my-topic";
  KafkaTopicParts *tps = kafka_tps_new(topic_buf, NULL);

  cr_assert_not_null(tps);
  cr_assert_str_eq(tps->topic, "my-topic");
  /* topic field must be an independent allocation, not a borrowed pointer */
  cr_assert_neq(tps->topic, topic_buf);

  /* mutating the source buffer must not affect the stored topic */
  topic_buf[0] = 'X';
  cr_assert_str_eq(tps->topic, "my-topic");

  kafka_tps_free(tps);
}

Test(kafka_topic_utils, tps_new_takes_ownership_of_partition_list)
{
  /* No leak / no double-free under valgrind: the partition list is owned by
   * the KafkaTopicParts after kafka_tps_new(); kafka_tps_free() releases it. */
  KafkaTopicParts *tps = kafka_tps_new("t", _make_partitions(3, 0, 1, 2));
  cr_assert_eq(g_list_length(tps->partitions), 3);
  kafka_tps_free(tps);
}

/* *********************
 *  Destruction
 * *********************/

Test(kafka_topic_utils, tps_free_handles_null_partitions)
{
  KafkaTopicParts *tps = kafka_tps_new("t", NULL);
  cr_assert_null(tps->partitions);
  kafka_tps_free(tps);
}

Test(kafka_topic_utils, tps_list_free_handles_null)
{
  /* g_list_free_full(NULL, ...) is documented safe in GLib */
  kafka_tps_list_free(NULL);
}

Test(kafka_topic_utils, tps_list_free_frees_multiple_entries)
{
  GList *l = NULL;
  l = g_list_append(l, kafka_tps_new("a", _make_partitions(1, 0)));
  l = g_list_append(l, kafka_tps_new("b", _make_partitions(2, 0, 1)));
  l = g_list_append(l, kafka_tps_new("c", NULL));
  kafka_tps_list_free(l);
}

/* *********************
 *  Equality
 * *********************/

Test(kafka_topic_utils, tps_equal_same_topic_same_partitions_returns_true)
{
  /* kafka_tps_equal() compares partition list elements by pointer identity
   * (parts1->data != parts2->data). This works in production only because
   * partition numbers are stored as GINT_TO_POINTER(int32_t) — identical
   * integers therefore produce identical pointer values. Any future change
   * that boxes partition numbers into allocated wrappers would silently
   * break equality even for logically identical lists. */
  KafkaTopicParts *a = kafka_tps_new("t", _make_partitions(2, 0, 1));
  KafkaTopicParts *b = kafka_tps_new("t", _make_partitions(2, 0, 1));

  cr_assert(kafka_tps_equal(a, b),
            "GINT_TO_POINTER(N) must produce identical pointers across calls");

  kafka_tps_free(a);
  kafka_tps_free(b);
}

Test(kafka_topic_utils, tps_equal_different_topic_returns_false)
{
  KafkaTopicParts *a = kafka_tps_new("topic-a", _make_partitions(1, 0));
  KafkaTopicParts *b = kafka_tps_new("topic-b", _make_partitions(1, 0));

  cr_assert_not(kafka_tps_equal(a, b));

  kafka_tps_free(a);
  kafka_tps_free(b);
}

Test(kafka_topic_utils, tps_equal_different_partition_count_returns_false)
{
  KafkaTopicParts *a = kafka_tps_new("t", _make_partitions(1, 0));
  KafkaTopicParts *b = kafka_tps_new("t", _make_partitions(2, 0, 1));

  cr_assert_not(kafka_tps_equal(a, b));

  kafka_tps_free(a);
  kafka_tps_free(b);
}

Test(kafka_topic_utils, tps_equal_different_partition_values_returns_false)
{
  KafkaTopicParts *a = kafka_tps_new("t", _make_partitions(2, 0, 1));
  KafkaTopicParts *b = kafka_tps_new("t", _make_partitions(2, 0, 2));

  cr_assert_not(kafka_tps_equal(a, b));

  kafka_tps_free(a);
  kafka_tps_free(b);
}

Test(kafka_topic_utils, tps_equal_empty_partition_lists_returns_true)
{
  KafkaTopicParts *a = kafka_tps_new("t", NULL);
  KafkaTopicParts *b = kafka_tps_new("t", NULL);

  cr_assert(kafka_tps_equal(a, b));

  kafka_tps_free(a);
  kafka_tps_free(b);
}

Test(kafka_topic_utils, tps_equal_partitions_in_different_order_returns_false)
{
  /* kafka_tps_equal() compares partition lists positionally
   * (see "We assume both lists are sorted" in kafka-topic-parts.c).
   * Same partitions in different order are NOT considered equal. */
  KafkaTopicParts *a = kafka_tps_new("t", _make_partitions(2, 0, 1));
  KafkaTopicParts *b = kafka_tps_new("t", _make_partitions(2, 1, 0));

  cr_assert_not(kafka_tps_equal(a, b));

  kafka_tps_free(a);
  kafka_tps_free(b);
}

/* *********************
 *  Pattern validation
 * *********************
 *
 * Topic identifiers in the source driver are validated as either strict
 * names (see test_kafka_topic.c) or — when that fails — as POSIX-extended
 * regex patterns via kafka_validate_topic_pattern(). The source driver
 * relies on this fallback to support wildcard topic subscriptions.
 */

/* Criterion parameter payloads must be self-contained — fixed-size arrays. */
struct topic_pattern_test_params
{
  gchar pattern[256];
};

ParameterizedTestParameters(kafka_topic_utils, valid_topic_pattern_tests)
{
  static struct topic_pattern_test_params params[] =
  {
    {"validname123"},
    {"valid.name-123"},
    {"topic.*"},
    {"^prefix_[0-9]+$"},
    {"foo|bar"},
    {"top[0-9]+"},
    {".+"},
  };

  return cr_make_param_array(struct topic_pattern_test_params, params, G_N_ELEMENTS(params));
}

ParameterizedTest(struct topic_pattern_test_params *param, kafka_topic_utils, valid_topic_pattern_tests)
{
  GError *error = NULL;
  cr_assert_eq(kafka_validate_topic_pattern(param->pattern, &error), TRUE,
               "expected `%s` to be a valid pattern", param->pattern);
  cr_assert_null(error);
}

ParameterizedTestParameters(kafka_topic_utils, invalid_topic_pattern_tests)
{
  static struct topic_pattern_test_params params[] =
  {
    {""},
    {"["},
    {"("},
    {"a[b"},
  };

  return cr_make_param_array(struct topic_pattern_test_params, params, G_N_ELEMENTS(params));
}

ParameterizedTest(struct topic_pattern_test_params *param, kafka_topic_utils, invalid_topic_pattern_tests)
{
  GError *error = NULL;
  cr_assert_eq(kafka_validate_topic_pattern(param->pattern, &error), FALSE,
               "expected `%s` to be an invalid pattern", param->pattern);
  if (error)
    g_error_free(error);
}

/* ************************
 *  Partition key format
 * ************************
 *
 * kafka_format_partition_key() builds the hash key used by the local persist
 * store (see _find_persist() / _partitions_persists_create() in
 * kafka-source-driver.c). Any change to the "<topic>#<partition>" format
 * would silently break offset recovery from on-disk state files for
 * upgraded users.
 */

struct partition_key_test_params
{
  gchar topic[256];
  int32_t partition;
  gchar expected[MAX_KAFKA_PARTITION_KEY_NAME_LEN];
};

ParameterizedTestParameters(kafka_topic_utils, format_partition_key_tests)
{
  static struct partition_key_test_params params[] =
  {
    {"plain",                0,          "plain#0"},
    {"plain",                -1,         "plain#-1"},
    {"topic.a",              42,         "topic.a#42"},
    {"with-dashes_and.dots", 2147483647, "with-dashes_and.dots#2147483647"},
  };

  return cr_make_param_array(struct partition_key_test_params, params, G_N_ELEMENTS(params));
}

ParameterizedTest(struct partition_key_test_params *param, kafka_topic_utils, format_partition_key_tests)
{
  gchar buf[MAX_KAFKA_PARTITION_KEY_NAME_LEN];
  gchar *result = kafka_format_partition_key(param->topic, param->partition, buf, sizeof(buf));

  cr_assert_eq(result, buf, "expected return value to point to the destination buffer");
  cr_assert_str_eq(buf, param->expected,
                   "expected key for topic=`%s` partition=%d to be `%s`, got `%s`",
                   param->topic, param->partition, param->expected, buf);
}

Test(kafka_topic_utils, format_partition_key_truncates_into_buffer)
{
  gchar fits[8]; /* exactly fits "abc#42\0" */
  kafka_format_partition_key("abc", 42, fits, sizeof(fits));
  cr_assert_str_eq(fits, "abc#42");

  /* g_snprintf truncates the formatted output to fit the buffer (left-anchored,
   * right-truncated) and always null-terminates. Pin both properties — a silent
   * change to either would corrupt persist-state keys for upgraded users. */
  gchar small[4]; /* forces truncation: "abc\0" */
  kafka_format_partition_key("abc", 42, small, sizeof(small));
  cr_assert_eq(strlen(small), sizeof(small) - 1,
               "g_snprintf must truncate and null-terminate within the buffer");
  cr_assert_str_eq(small, "abc",
                   "truncation must be right-side: keep the prefix, drop the tail");
}
