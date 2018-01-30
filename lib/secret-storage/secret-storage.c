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

#include <errno.h>
#include <string.h>
#include <stddef.h>

#include "nondumpable-allocator.h"
#include "secret-storage.h"

#define SECRET_HEADER_SIZE offsetof(Secret, data)
#define SECRET_STORAGE_HEADER_SIZE (SECRET_HEADER_SIZE + offsetof(SecretStorage, secret))
#define SECRET_STORAGE_INITIAL_SIZE 2048

typedef struct
{
  SecretStorageCB func;
  gpointer user_data;
} Subscription;

typedef struct
{
  GArray *subscriptions;
  Secret secret;
} SecretStorage;

GHashTable *secret_manager INTERNAL;
volatile gint secret_manager_uninitialized INTERNAL = 1;

static SecretStorage *
secret_storage_new(gsize len)
{
  SecretStorage *storage = nondumpable_buffer_alloc(len + SECRET_STORAGE_HEADER_SIZE);
  return storage;
}

static void
secret_storage_free(SecretStorage *self)
{
  g_array_free(self->subscriptions, TRUE);
  nondumpable_buffer_free(self);
}

void
secret_storage_init()
{
  if (g_atomic_int_dec_and_test(&secret_manager_uninitialized))
    {
      secret_manager = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                             (GDestroyNotify)secret_storage_free);
      g_assert(secret_manager);
    }
  else
    g_assert_not_reached();
}

void
secret_storage_deinit()
{
  g_assert(!secret_manager_uninitialized);
  g_hash_table_destroy(secret_manager);
  secret_manager = NULL;
}

static void
write_secret(SecretStorage *storage, gchar *secret, gsize len)
{
  storage->secret.len = len;
  nondumpable_memcpy(storage->secret.data, secret, len);
}

static SecretStorage *
overwrite_secret(SecretStorage *storage, gchar *secret, gsize len)
{
  memset(storage->secret.data, 0, storage->secret.len);
  write_secret(storage, secret, len);
  return storage;
}

static SecretStorage *
realloc_and_write_secret(SecretStorage *secret_storage, gchar *key, gchar *secret, gsize len)
{
  SecretStorage *maybe_new_storage = nondumpable_buffer_realloc(secret_storage, len);
  write_secret(maybe_new_storage, secret, len);
  if (secret_storage != maybe_new_storage)
    g_hash_table_insert(secret_manager, strdup(key), maybe_new_storage);
  return maybe_new_storage;
}

static SecretStorage *
update_storage_with_secret(SecretStorage *secret_storage, gchar *key, gchar *secret, gsize len)
{
  gboolean fits_into_storage = secret_storage->secret.len > len;
  if (fits_into_storage)
    return overwrite_secret(secret_storage, secret, len);
  else
    return realloc_and_write_secret(secret_storage, key, secret, len);
}

static SecretStorage *
create_secret_storage_with_secret(gchar *key, gchar *secret, gsize len)
{
  SecretStorage *secret_storage = secret_storage_new(len);
  if (!secret_storage)
    return NULL;
  secret_storage->secret.len = len;
  nondumpable_memcpy(&secret_storage->secret.data, secret, len);
  g_hash_table_insert(secret_manager, strdup(key), secret_storage);
  secret_storage->subscriptions = g_array_new(FALSE, FALSE, sizeof(Subscription));

  return secret_storage;
}

static void
run_callbacks_initiate(gchar *key, GArray *subscriptions)
{
  static gboolean initiated = FALSE;

  if (initiated)
    return;

  initiated = TRUE;
  gsize original_length = subscriptions->len;
  for (int i = 0; i < original_length; i++)
    {
      Subscription sub = g_array_index(subscriptions, Subscription, i);
      secret_storage_with_secret(key, sub.func, sub.user_data);
    }
  g_array_remove_range(subscriptions, 0, original_length);
  initiated = FALSE;
}

gboolean
secret_storage_store_secret(gchar *key, gchar *secret, gsize len)
{
  if (!secret)
    len = 0;
  else if (len == -1)
    len = strlen(secret) + 1;

  SecretStorage *secret_storage;
  secret_storage = g_hash_table_lookup(secret_manager, key);
  if (secret_storage)
    secret_storage = update_storage_with_secret(secret_storage, key, secret, len);
  else
    secret_storage = create_secret_storage_with_secret(key, secret, len);

  if (!secret_storage)
    return FALSE;

  run_callbacks_initiate(key, secret_storage->subscriptions);

  return TRUE;
}

gboolean
secret_storage_store_string(gchar *key, gchar *secret)
{
  return secret_storage_store_secret(key, secret, -1);
}

Secret *secret_storage_clone_secret(Secret *self)
{
  Secret *copy = nondumpable_buffer_alloc(self->len + SECRET_HEADER_SIZE);
  copy->len = self->len;
  nondumpable_memcpy(copy->data, self->data, self->len);
  return copy;
}

Secret *
secret_storage_get_secret_by_name(gchar *key)
{
  SecretStorage *secret_storage = g_hash_table_lookup(secret_manager, key);
  if (!secret_storage)
    return NULL;
  return secret_storage_clone_secret(&secret_storage->secret);
}

void
secret_storage_put_secret(Secret *self)
{
  nondumpable_buffer_free(self);
}

void
secret_storage_with_secret(gchar *key, SecretStorageCB func, gpointer user_data)
{
  Secret *secret = secret_storage_get_secret_by_name(key);
  func(secret, user_data);
  secret_storage_put_secret(secret);
}

static gboolean
insert_empty_secret_storage(gchar *key)
{
  return secret_storage_store_string(key, NULL);
}

gboolean
secret_storage_subscribe_for_key(gchar *key, SecretStorageCB func, gpointer user_data)
{

  SecretStorage *secret_storage;
  if (!g_hash_table_contains(secret_manager, key))
    if (!insert_empty_secret_storage(key))
      return FALSE;

  secret_storage = g_hash_table_lookup(secret_manager, key);
  Subscription new_subscription = {.func = func, .user_data = user_data};
  g_array_append_val(secret_storage->subscriptions, new_subscription);

  if (secret_storage->secret.len != 0)
    {
      run_callbacks_initiate(key, secret_storage->subscriptions);
    }

  return TRUE;
}

typedef struct
{
  SecretStatusCB func;
  gpointer user_data;
} SecretCallBackAction;

static gboolean
run_callback_for_status(gpointer key, gpointer value, gpointer user_data)
{
  SecretCallBackAction *action = (SecretCallBackAction *)user_data;
  gchar *key_with_obscured_location = g_strdup(key);
  SecretStatus secret_status = {.key = key_with_obscured_location};
  gboolean should_continue = !action->func(&secret_status, action->user_data);
  g_free(key_with_obscured_location);

  return should_continue;
}

void
secret_store_status_foreach(SecretStatusCB cb, gpointer user_data)
{
  SecretCallBackAction action = {.func = cb, .user_data = user_data};
  g_hash_table_find(secret_manager, run_callback_for_status, &action);
}
