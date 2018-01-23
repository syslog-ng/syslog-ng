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
  /* Placeholder for nonce, refcount */
  Secret secret;
} SecretStorage;

GHashTable *secret_manager INTERNAL;
volatile gint secret_manager_uninitialized INTERNAL = 1;

static SecretStorage *
secret_storage_new(gsize len)
{
  g_assert(len > 0);
  SecretStorage *storage = nondumpable_buffer_alloc(len + SECRET_STORAGE_HEADER_SIZE);
  return storage;
}

static void
secret_storage_free(SecretStorage *self)
{
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

SecretStorageCB g_func;
gpointer g_user_data;

gboolean
secret_storage_store_secret(gchar *key, gchar *secret, gsize len)
{
  if (len == -1)
    len = strlen(secret) + 1;

  SecretStorage *secret_storage = secret_storage_new(len);
  if (!secret_storage)
    return FALSE;

  secret_storage->secret.len = len;
  nondumpable_memcpy(&secret_storage->secret.data, secret, len);
  g_hash_table_insert(secret_manager, strdup(key), secret_storage);

  if (g_func)
    secret_storage_with_secret(key, g_func, g_user_data);

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

gboolean
secret_storage_subscribe_for_key(gchar *key, SecretStorageCB func, gpointer user_data)
{
  g_func = func;
  g_user_data = user_data;

  if (secret_storage_get_secret_by_name(key))
    {
      secret_storage_with_secret(key, g_func, g_user_data);
    }

  return TRUE;
}
