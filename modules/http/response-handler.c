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
 *
 */

#include "response-handler.h"

HttpResult
http_result_success(gpointer user_data)
{
  return HTTP_RESULT_SUCCESS;
}

HttpResult
http_result_retry(gpointer user_data)
{
  return HTTP_RESULT_RETRY;
}

HttpResult
http_result_drop(gpointer user_data)
{
  return HTTP_RESULT_DROP;
}

HttpResult
http_result_disconnect(gpointer user_data)
{
  return HTTP_RESULT_DISCONNECT;
}

static void
_free_entry(HttpResponseHandler *self)
{
  g_free(self);
}

HttpResponseHandlers *
http_response_handlers_new(void)
{
  return g_hash_table_new_full(g_int_hash, g_int_equal, g_free, (GDestroyNotify)_free_entry);
}

void
http_response_handlers_free(HttpResponseHandlers *self)
{
  g_hash_table_destroy(self);
}

static HttpResponseHandler *
_clone(HttpResponseHandler *self)
{
  HttpResponseHandler *clone = g_new(HttpResponseHandler, 1);
  *clone = *self;
  return clone;
}

void
http_response_handlers_insert(HttpResponseHandlers *self, HttpResponseHandler *response_handler)
{
  HttpResponseHandler *clone = _clone(response_handler);

  /* It migth be tempting to use status code from the clone, hence we
  neither need to allocate a gint, nor need a key free function in the
  hashtable. The problem is, when glib overwrites the value behind the
  same key, by default it does not replace the old key with the new,
  only the value changes. In our case, the value will be deallocated,
  hence the key in the hashtable becomes invalid. */

  gint *status_code = g_new(gint, 1);
  *status_code = clone->status_code;
  g_hash_table_insert(self, status_code, clone);
}

HttpResponseHandler *
http_response_handlers_lookup(HttpResponseHandlers *self, glong status_code)
{
  return g_hash_table_lookup(self, &status_code);
}
