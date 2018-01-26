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

#ifndef SECRET_STORAGE_H_INCLUDED
#define SECRET_STORAGE_H_INCLUDED

#include "lib/compat/glib.h"

#define PUBLIC __attribute__ ((visibility ("default")))
#define INTERNAL __attribute__ ((visibility ("hidden")))

typedef struct
{
  gsize len;
  gchar data[];
} Secret;

typedef void(*SecretStorageCB)(Secret *secret, gpointer user_data);

void secret_storage_init(void) PUBLIC;
void secret_storage_deinit(void) PUBLIC;

gboolean secret_storage_store_string(gchar *key, gchar *secret) PUBLIC;
gboolean secret_storage_store_secret(gchar *key, gchar *secret, gsize len) PUBLIC;

void secret_storage_with_secret(gchar *key, SecretStorageCB func, gpointer user_data) PUBLIC;
Secret *secret_storage_get_secret_by_name(gchar *key) PUBLIC;
void secret_storage_put_secret(Secret *self) PUBLIC;
Secret *secret_storage_clone_secret(Secret *self) PUBLIC;

gboolean secret_storage_subscribe_for_key(gchar *key, SecretStorageCB func, gpointer user_data) PUBLIC;

typedef struct
{
  gchar *key;
} SecretStatus;
typedef gboolean (*SecretStatusCB)(SecretStatus *secret_status, gpointer user_data);
void secret_storage_status_foreach(SecretStatusCB cb, gpointer user_data) PUBLIC;

#endif
