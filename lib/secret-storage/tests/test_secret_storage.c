/*
 * Copyright (c) 2018 Balabit
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

#include "secret-storage.h"

TestSuite(secretstorage, .init = secret_storage_init, .fini = secret_storage_deinit);

Test(secretstorage, simple_store_get)
{
  secret_storage_store_secret("key1", "value1", -1);
  Secret *secret = secret_storage_get_secret_by_name("key1");
  cr_assert_str_eq(secret->data, "value1");
  secret_storage_put_secret(secret);
}

void secret_checker(Secret *secret, gpointer expected)
{
  cr_assert_str_eq(expected, secret->data);
}

Test(secretstorage, simple_store_with)
{
  secret_storage_store_secret("key1", "value1", -1);
  secret_storage_with_secret("key1", secret_checker, "value1");
}

Test(secretstorage, simple_store_single_string)
{
  secret_storage_store_string("key1", "value1");
  Secret *secret = secret_storage_get_secret_by_name("key1");
  cr_assert_str_eq(secret->data, "value1");
  secret_storage_put_secret(secret);
}
