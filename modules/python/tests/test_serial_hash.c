/*
 * Copyright (c) 2019 Balabit
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
 */

#include "serial-hash.h"
#include "serial-hash-internal.h"
#include <criterion/criterion.h>

void setup(void)
{
}

void teardown(void)
{
}

TestSuite(serial_hash, .init = setup, .fini = teardown);

Test(serial_hash, test_serial_hash)
{
  gchar buffer[400];

  SerialHash *self = serial_hash_new((guchar *)buffer, sizeof(buffer));

  serial_hash_insert(self, "key", (guchar *)"value", sizeof("value"));

  const guchar *data = NULL;
  gsize data_len = 0;
  serial_hash_lookup(self, "key", &data, &data_len);
  cr_assert_str_eq((gchar *)data, "value");
  cr_assert_eq(data_len, sizeof("value"));

  serial_hash_free(self);
}

Test(serial_hash, test_payload)
{
  guchar *payload = NULL;
  gsize payload_len = 0;

  payload_new("key", (guchar *)"value", sizeof("value"), &payload, &payload_len);
  cr_assert(payload);
  cr_assert(payload_len);

  cr_assert_str_eq(payload_get_key(payload), "key");

  guchar *value = NULL;
  gsize value_len = 0;
  payload_get_value(payload, &value, &value_len);
  cr_assert_eq(value_len, sizeof("value"));
  cr_assert_str_eq((gchar *)value, "value");
  payload_free(payload);
}

Test(serial_hash, test_serial_remove)
{
  gchar buffer[400];

  SerialHash *self = serial_hash_new((guchar *)buffer, sizeof(buffer));

  const guchar *data = NULL;
  gsize data_len = 0;
  serial_hash_lookup(self, "key", &data, &data_len);
  cr_assert_null(data);
  cr_assert_eq(data_len, 0);

  serial_hash_insert(self, "key", (guchar *)"value", sizeof("value"));
  serial_hash_lookup(self, "key", &data, &data_len);
  cr_assert_str_eq((gchar *)data, "value");
  cr_assert_eq(data_len, sizeof("value"));

  serial_hash_free(self);
}

Test(serial_hash, test_serial_update)
{
  gchar buffer[400];

  SerialHash *self = serial_hash_new((guchar *)buffer, sizeof(buffer));

  const guchar *data = NULL;
  gsize data_len = 0;
  serial_hash_insert(self, "key", (guchar *)"value", sizeof("value"));
  serial_hash_lookup(self, "key", &data, &data_len);
  cr_assert_str_eq((gchar *)data, "value");
  cr_assert_eq(data_len, sizeof("value"));

  serial_hash_insert(self, "key", (guchar *)"value-longer", sizeof("value-longer"));
  serial_hash_lookup(self, "key", &data, &data_len);
  cr_assert_str_eq((gchar *)data, "value-longer");
  cr_assert_eq(data_len, sizeof("value-longer"));

  serial_hash_insert(self, "key", (guchar *)"v", sizeof("v"));
  serial_hash_lookup(self, "key", &data, &data_len);
  cr_assert_str_eq((gchar *)data, "v");
  cr_assert_eq(data_len, sizeof("v"));

  serial_hash_free(self);
}

Test(serial_hash, test_serial_multi_insert)
{
  gchar buffer[400];

  SerialHash *self = serial_hash_new((guchar *)buffer, sizeof(buffer));

  const guchar *data = NULL;
  gsize data_len = 0;
  serial_hash_insert(self, "key1", (guchar *)"value1", sizeof("value1"));
  serial_hash_insert(self, "key2", (guchar *)"value2", sizeof("value2"));
  serial_hash_insert(self, "key3", (guchar *)"value3", sizeof("value3"));

  serial_hash_lookup(self, "key1", &data, &data_len);
  cr_assert_str_eq((gchar *)data, "value1");
  cr_assert_eq(data_len, sizeof("value1"));

  serial_hash_lookup(self, "key2", &data, &data_len);
  cr_assert_str_eq((gchar *)data, "value2");
  cr_assert_eq(data_len, sizeof("value2"));

  serial_hash_lookup(self, "key3", &data, &data_len);
  cr_assert_str_eq((gchar *)data, "value3");
  cr_assert_eq(data_len, sizeof("value3"));

  serial_hash_free(self);
}
