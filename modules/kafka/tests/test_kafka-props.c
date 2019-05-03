/*
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Balazs Scheidler
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
#include "kafka-props.h"
#include "messages.h"
#include "logmsg/logmsg.h"
#include <criterion/criterion.h>
#include "grab-logging.h"


#define TESTDATA_DIR TOP_SRCDIR "/modules/kafka/tests/"

static KafkaProperty *
_get_nth_prop(GList *prop_list, gint n)
{
  cr_assert(prop_list != NULL);
  return (KafkaProperty *) g_list_nth(prop_list, n)->data;
}

static void
_assert_nth_prop_equals(GList *prop_list, gint n, const gchar *name, const gchar *value)
{
  KafkaProperty *prop = _get_nth_prop(prop_list, n);
  cr_assert_str_eq(prop->name, name,
                   "name does not match in the %d. element of the property, value=%s, expected=%s",
                   n, prop->name, name);
  cr_assert_str_eq(prop->value, value,
                   "value does not match in the %d. element of the property, value=%s, expected=%s",
                   n, prop->value, value);
}

Test(kafka_props, kafka_prop_new_allocates_a_prop)
{
  KafkaProperty *prop;

  prop = kafka_property_new("name", "value");
  cr_assert_str_eq(prop->name, "name");
  cr_assert_str_eq(prop->value, "value");
  kafka_property_free(prop);
}

Test(kafka_props, kafka_prop_list_allocation_and_destruction)
{
  GList *pl = NULL;

  pl = g_list_append(pl, kafka_property_new("name1", "value1"));
  pl = g_list_append(pl, kafka_property_new("name2", "value2"));
  pl = g_list_append(pl, kafka_property_new("name3", "value3"));

  _assert_nth_prop_equals(pl, 2, "name3", "value3");
  _assert_nth_prop_equals(pl, 1, "name2", "value2");
  _assert_nth_prop_equals(pl, 0, "name1", "value1");

  kafka_property_list_free(pl);
}

Test(kafka_props, kafka_read_properties_file_gets_keys_from_the_file)
{
  GList *pl = kafka_read_properties_file(TESTDATA_DIR "sample.properties");

  _assert_nth_prop_equals(pl, 0, "name1", "value1");
  _assert_nth_prop_equals(pl, 1, "name2", "value2");
  _assert_nth_prop_equals(pl, 2, "name3", "value3");
  _assert_nth_prop_equals(pl, 3, "name4",
                          "continuation this continues on this and this line \\and this        but not on this \\");
  _assert_nth_prop_equals(pl, 4, "name5", "value5");
  _assert_nth_prop_equals(pl, 5, "namethatincludesequal=sign", "value");
  _assert_nth_prop_equals(pl, 6, "namethatincludenewline\nhere", "value");
  _assert_nth_prop_equals(pl, 7, "namethatincludetab\there", "value");
  _assert_nth_prop_equals(pl, 8, "namethatincludeunicode@here", "value");
  kafka_property_list_free(pl);
}

Test(kafka_props, kafka_translate_properties_returns_unknown_properties_unmodified)
{
  GList *pl = NULL;

  pl = g_list_append(pl, kafka_property_new("name1", "value1"));
  pl = g_list_append(pl, kafka_property_new("name2", "value2"));
  pl = g_list_append(pl, kafka_property_new("name3", "value3"));

  pl = kafka_translate_java_properties(pl);

  _assert_nth_prop_equals(pl, 2, "name3", "value3");
  _assert_nth_prop_equals(pl, 1, "name2", "value2");
  _assert_nth_prop_equals(pl, 0, "name1", "value1");
  kafka_property_list_free(pl);
}

Test(kafka_props, kafka_translate_ssl_endpoint_identification_algorithm_fails_if_set_to_non_empty)
{
  GList *pl = NULL;

  pl = g_list_append(pl, kafka_property_new("ssl.endpoint.identification.algorithm", "HTTPS"));
  start_grabbing_messages();
  pl = kafka_translate_java_properties(pl);
  assert_grabbed_log_contains("unsupported java property");
  assert_grabbed_log_contains("ssl.endpoint.identification.algorithm");
  assert_grabbed_log_contains("HTTPS");
  stop_grabbing_messages();
  cr_assert_null(pl);
}

Test(kafka_props, kafka_translate_sasl_jaas_config_extracts_username_into_sasl_username_and_password)
{
  GList *pl = NULL;

  pl = g_list_append(pl, kafka_property_new("sasl.jaas.config",
                                            "org.apache.kafka.common.security.plain.PlainLoginModule required username=\"foo\" password=\"bar\""));
  pl = kafka_translate_java_properties(pl);
  _assert_nth_prop_equals(pl, 0, "sasl.username", "foo");
  _assert_nth_prop_equals(pl, 1, "sasl.password", "bar");

  kafka_property_list_free(pl);
}

Test(kafka_props, kafka_translate_ssl_endpoint_identification_algorithm_accepts_empty)
{
  GList *pl = NULL;

  pl = g_list_append(pl, kafka_property_new("ssl.endpoint.identification.algorithm", ""));
  pl = kafka_translate_java_properties(pl);
  cr_assert_not_null(pl);
  kafka_property_list_free(pl);
}

static void
setup(void)
{
  msg_init(FALSE);
  log_msg_global_init();
}

static void
teardown(void)
{
  msg_deinit();
}

TestSuite(kafka_props, .init = setup, .fini = teardown);
