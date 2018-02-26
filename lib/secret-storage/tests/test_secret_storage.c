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
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>

#include "secret-storage.h"
#include "nondumpable-allocator.h"

void
logger(char *summary, char *reason)
{
  fprintf(stderr, "%s : reason=%s", summary, reason);
}

void
secret_storage_testsuite_init()
{
  secret_storage_init();
  nondumpable_setlogger(logger);
}

void
secret_storage_testsuite_deinit()
{
  secret_storage_deinit();
}


TestSuite(secretstorage, .init = secret_storage_testsuite_init, .fini = secret_storage_testsuite_deinit);

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

Test(secretstorage, store_multiple_secrets)
{
  secret_storage_store_string("key1", "value1");
  secret_storage_store_string("key2", "value2");
  Secret *secret1 = secret_storage_get_secret_by_name("key1");
  cr_assert_str_eq(secret1->data, "value1");
  Secret *secret2 = secret_storage_get_secret_by_name("key2");
  cr_assert_str_eq(secret2->data, "value2");

  secret_storage_put_secret(secret1);
  secret_storage_put_secret(secret2);
}

Test(secretstorage, read_nonexistent_secret)
{
  Secret *secret = secret_storage_get_secret_by_name("key");
  cr_assert_eq(secret, NULL);
}

Test(secretstorage, store_secret_with_embedded_zero)
{
  secret_storage_store_secret("key", "a\0b", 4);
  Secret *secret = secret_storage_get_secret_by_name("key");
  gint result = memcmp(secret->data, "a\0b", 4);
  cr_assert_eq(result, 0);
  secret_storage_put_secret(secret);
}

void set_variable_to_true_cb(Secret *secret, gpointer user_data)
{
  *((gboolean *)user_data) = TRUE;
}

Test(secretstorage, subscribe_before_store)
{
  gboolean test_variable = FALSE;
  secret_storage_subscribe_for_key("key", set_variable_to_true_cb, &test_variable);
  cr_assert_not(test_variable);
  secret_storage_store_string("key", "secret");
  cr_assert(test_variable);
}

Test(secretstorage, subscribe_after_store)
{
  gboolean test_variable = FALSE;
  secret_storage_store_string("key", "secret");
  secret_storage_subscribe_for_key("key", set_variable_to_true_cb, &test_variable);
  cr_assert(test_variable);
}

Test(secretstorage, subscriptions_per_keys)
{
  gboolean key1_test_variable = FALSE;
  gboolean key2_test_variable = FALSE;
  secret_storage_subscribe_for_key("key1", set_variable_to_true_cb, &key1_test_variable);
  secret_storage_subscribe_for_key("key2", set_variable_to_true_cb, &key2_test_variable);
  cr_assert_not(key1_test_variable);
  cr_assert_not(key2_test_variable);

  secret_storage_store_string("key1", "secret");
  cr_assert(key1_test_variable);
  cr_assert_not(key2_test_variable);

  secret_storage_store_string("key2", "secret");
  cr_assert(key1_test_variable);
  cr_assert(key2_test_variable);
}

Test(secretstorage, two_subscribe_without_store)
{
  gboolean test_variable = FALSE;
  secret_storage_subscribe_for_key("key", set_variable_to_true_cb, &test_variable);
  secret_storage_subscribe_for_key("key", set_variable_to_true_cb, &test_variable);
  cr_assert_not(test_variable);
}

void
check_secret(Secret *secret, gpointer user_data)
{
  cr_assert_str_eq(secret->data, user_data);
}

Test(secretstorage, subscribe_cb_check_secret)
{
  secret_storage_subscribe_for_key("key", check_secret, "secret");
  secret_storage_store_string("key", "secret");
}

Test(secretstorage, multiple_subscriptions_for_same_key)
{
  gboolean key1_test_variable = FALSE;
  gboolean key2_test_variable = FALSE;
  secret_storage_subscribe_for_key("key", set_variable_to_true_cb, &key1_test_variable);
  secret_storage_subscribe_for_key("key", set_variable_to_true_cb, &key2_test_variable);
  cr_assert_not(key1_test_variable);
  cr_assert_not(key2_test_variable);

  secret_storage_store_string("key", "secret");
  cr_assert(key1_test_variable);
  cr_assert(key2_test_variable);
}

Test(secretstorage, subscription_reset_after_called)
{
  gboolean key_test_variable = FALSE;
  secret_storage_subscribe_for_key("key", set_variable_to_true_cb, &key_test_variable);
  secret_storage_store_string("key", "secret");
  cr_assert(key_test_variable);

  key_test_variable = FALSE;
  secret_storage_store_string("key", "secret");
  cr_assert_not(key_test_variable);
}

typedef struct
{
  gpointer user_data;
  gpointer evidence;
} UserDataWithEvidence;

gboolean
check_status_callback(SecretStatus *secret_status, gpointer user_data)
{
  cr_assert_str_eq(secret_status->key, ((UserDataWithEvidence *)user_data)->user_data);
  gboolean *evidence = ((UserDataWithEvidence *)user_data)->evidence;
  *evidence = TRUE;
  return TRUE;
}

Test(secretstorage, check_secret_status)
{
  gboolean test_variable = FALSE;
  secret_storage_store_string("key", "secret");
  UserDataWithEvidence user_data = {.user_data = "key", .evidence = &test_variable};
  secret_storage_status_foreach(check_status_callback, &user_data);
  cr_assert(test_variable);
}

gboolean
stop_in_the_middle_callback(SecretStatus *secret_status, gpointer user_data)
{
  gint *test_variable = user_data;
  gboolean should_continue = (1 != *test_variable);
  (*test_variable)++;
  return should_continue;
}

Test(secretstorage, secret_status_can_stop_in_the_middle)
{
  gint test_variable = 0;
  secret_storage_store_string("key1", "secret");
  secret_storage_store_string("key2", "secret");
  secret_storage_store_string("key3", "secret");
  secret_storage_store_string("key4", "secret");
  secret_storage_status_foreach(stop_in_the_middle_callback, &test_variable);
  cr_assert_eq(test_variable, 2);
}

void
subscribe_until_success(Secret *secret, gpointer user_data)
{
  if (strcmp(secret->data, "good_password"))
    {
      secret_storage_subscribe_for_key("key", subscribe_until_success, user_data);
      return;
    }
  *((gboolean *)user_data) = TRUE;
}

Test(secretstorage, subscribe_until_success)
{
  gboolean test_variable = FALSE;
  secret_storage_subscribe_for_key("key", subscribe_until_success, &test_variable);
  secret_storage_store_string("key", "wrong_password");
  cr_assert_not(test_variable);
  secret_storage_store_string("key", "good_password");
  cr_assert(test_variable);
}

Test(secretstorage, test_rlimit)
{
  struct rlimit locked_limit;
  cr_assert(!getrlimit(RLIMIT_MEMLOCK, &locked_limit));
  locked_limit.rlim_cur = MIN(locked_limit.rlim_max, 64 * 1024);
  cr_assert(!setrlimit(RLIMIT_MEMLOCK, &locked_limit));
  const gsize PAGESIZE = sysconf(_SC_PAGE_SIZE);

  gchar *key_fmt = g_strdup("keyXXX");
  int i = 0;
  int for_limit = locked_limit.rlim_cur/PAGESIZE;
  for (; i < for_limit; i++)
    {
      sprintf(key_fmt, "key%03d", i);
      cr_assert(secret_storage_store_string(key_fmt, "value"), "offending_key: %s, for_limit: %d", key_fmt, for_limit);
    }

  sprintf(key_fmt, "key%03d", i);

  /* root is not restricted by rlimit */
  if (geteuid() == 0)
    {
      cr_assert(secret_storage_store_string(key_fmt, "value"), "offending_key: %s", key_fmt);
      cr_assert(secret_storage_subscribe_for_key("key000", secret_checker, "value"));
    }
  else
    {
      cr_assert_not(secret_storage_store_string(key_fmt, "value"), "offending_key: %s", key_fmt);
    }
}

static void
update_state_callback(Secret *secret, gpointer user_data)
{
  secret_storage_update_status("key", SECRET_STORAGE_STATUS_INVALID_PASSWORD);
}

static gboolean
assert_invalid_password_state(SecretStatus *secret_status, gpointer user_data)
{
  cr_assert_eq(secret_status->state, SECRET_STORAGE_STATUS_INVALID_PASSWORD);
  return FALSE;
}

Test(secretstorage, test_state_update)
{
  secret_storage_subscribe_for_key("key", update_state_callback, NULL);
  secret_storage_store_string("key", "wrong_password");
  secret_storage_status_foreach(assert_invalid_password_state, NULL);
}
