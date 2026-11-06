/*
 * Copyright (c) 2026 Akos Zalavary <akos.zalavary@logiqum.com>
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
  return g_hash_table_new_full(g_int_hash, g_int_equal, NULL, (GDestroyNotify)_free_entry);
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
  g_hash_table_replace(self, &clone->status_code, clone);
}

HttpResponseHandler *
http_response_handlers_lookup(HttpResponseHandlers *self, glong status_code)
{
  return g_hash_table_lookup(self, &status_code);
}

#define HTTP_RESPONSE_BODY_HANDLER_ERROR http_response_body_handler_error_quark()

static GQuark
http_response_body_handler_error_quark(void)
{
  return g_quark_from_static_string("http-response-body-handler");
}

struct _HttpResponseBodyHandlers
{
  GList *handlers;
};

static void
_body_handler_free(HttpResponseBodyHandler *self)
{
  if (self->pattern)
    pcre2_code_free(self->pattern);
  g_free(self->pattern_str);
  g_free(self);
}

HttpResponseBodyHandlers *
http_response_body_handlers_new(void)
{
  return g_new0(HttpResponseBodyHandlers, 1);
}

void
http_response_body_handlers_free(HttpResponseBodyHandlers *self)
{
  g_list_free_full(self->handlers, (GDestroyNotify) _body_handler_free);
  g_free(self);
}

gboolean
http_response_body_handlers_append(HttpResponseBodyHandlers *self, const gchar *pattern,
                                   HttpResponseAction action, GError **error)
{
  gint rc;
  PCRE2_SIZE erroffset;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  pcre2_code *compiled = pcre2_compile((PCRE2_SPTR) pattern, PCRE2_ZERO_TERMINATED, 0, &rc, &erroffset, NULL);
  if (!compiled)
    {
      PCRE2_UCHAR error_message[128];

      pcre2_get_error_message(rc, error_message, sizeof(error_message));
      g_set_error(error, HTTP_RESPONSE_BODY_HANDLER_ERROR, 0,
                  "Error compiling response-body-action() regexp, error=%s, error_at=%" G_GSIZE_FORMAT,
                  (gchar *) error_message, erroffset);
      return FALSE;
    }
  pcre2_jit_compile(compiled, PCRE2_JIT_COMPLETE);

  HttpResponseBodyHandler *handler = g_new0(HttpResponseBodyHandler, 1);
  handler->pattern_str = g_strdup(pattern);
  handler->pattern = compiled;
  handler->action = action;
  self->handlers = g_list_append(self->handlers, handler);
  return TRUE;
}

gboolean
http_response_body_handlers_is_empty(HttpResponseBodyHandlers *self)
{
  return self->handlers == NULL;
}

static gboolean
_body_handler_matches(HttpResponseBodyHandler *self, const gchar *body, gsize body_len)
{
  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(self->pattern, NULL);
  gint rc = pcre2_match(self->pattern, (PCRE2_SPTR) body, (PCRE2_SIZE) body_len, 0, 0, match_data, NULL);
  pcre2_match_data_free(match_data);
  return rc >= 0;
}

HttpResponseBodyHandler *
http_response_body_handlers_lookup(HttpResponseBodyHandlers *self, const gchar *body, gsize body_len)
{
  for (GList *l = self->handlers; l; l = l->next)
    {
      HttpResponseBodyHandler *handler = (HttpResponseBodyHandler *) l->data;
      if (_body_handler_matches(handler, body, body_len))
        return handler;
    }
  return NULL;
}

