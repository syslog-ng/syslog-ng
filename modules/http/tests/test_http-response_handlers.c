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

#include <criterion/criterion.h>

#include "response-handler.h"
#include <string.h>

Test(response_handlers, test_response_handlers)
{
  HttpResponseHandlers *self = http_response_handlers_new();

  cr_assert_null(http_response_handlers_lookup(self, 404));

  HttpResponseHandler response_handler =  { .status_code = 404 };
  http_response_handlers_insert(self, &response_handler);
  cr_assert(http_response_handlers_lookup(self, 404));

  http_response_handlers_free(self);

}

HttpResult
action404(gpointer user_data)
{
  return HTTP_RESULT_SUCCESS;
}

HttpResult
action505(gpointer user_data)
{
  return HTTP_RESULT_SUCCESS;
}


Test(response_handlers, test_response_handlers_multiple_entries)
{
  HttpResponseHandlers *self = http_response_handlers_new();

  cr_assert_null(http_response_handlers_lookup(self, 404));

  HttpResponseHandler response_handler404 =  { .status_code = 404, .action = action404};
  HttpResponseHandler response_handler505 =  { .status_code = 505, .action = action505};
  http_response_handlers_insert(self, &response_handler404);
  http_response_handlers_insert(self, &response_handler505);
  cr_assert(http_response_handlers_lookup(self, 404));
  cr_assert_eq(http_response_handlers_lookup(self, 404)->action, action404);
  cr_assert(http_response_handlers_lookup(self, 505));
  cr_assert_eq(http_response_handlers_lookup(self, 505)->action, action505);
  cr_assert_null(http_response_handlers_lookup(self, 101));

  http_response_handlers_free(self);

}

Test(response_body_handlers, test_empty_list_matches_nothing)
{
  HttpResponseBodyHandlers *self = http_response_body_handlers_new();

  cr_assert(http_response_body_handlers_is_empty(self));
  cr_assert_null(http_response_body_handlers_lookup(self, "{\"Status\": \"Fail\"}", 18));

  http_response_body_handlers_free(self);
}

Test(response_body_handlers, test_first_matching_pattern_wins)
{
  HttpResponseBodyHandlers *self = http_response_body_handlers_new();

  cr_assert(http_response_body_handlers_append(self, "DATA_QUALITY_ERROR", http_result_drop, NULL));
  cr_assert(http_response_body_handlers_append(self, "\"Status\":\\s*\"Fail\"", http_result_retry, NULL));
  cr_assert_not(http_response_body_handlers_is_empty(self));

  const gchar *quality = "{\"Status\": \"Fail\", \"Message\": \"[DATA_QUALITY_ERROR]too many filtered rows\"}";
  HttpResponseBodyHandler *handler = http_response_body_handlers_lookup(self, quality, strlen(quality));
  cr_assert_not_null(handler);
  cr_assert_eq(handler->action(NULL), HTTP_RESULT_DROP);

  const gchar *unavailable = "{\"Status\": \"Fail\", \"Message\": \"No available backends\"}";
  handler = http_response_body_handlers_lookup(self, unavailable, strlen(unavailable));
  cr_assert_not_null(handler);
  cr_assert_eq(handler->action(NULL), HTTP_RESULT_RETRY);

  const gchar *success = "{\"Status\": \"Success\", \"Message\": \"OK\"}";
  cr_assert_null(http_response_body_handlers_lookup(self, success, strlen(success)));

  http_response_body_handlers_free(self);
}

Test(response_body_handlers, test_match_is_limited_to_the_given_length)
{
  HttpResponseBodyHandlers *self = http_response_body_handlers_new();

  cr_assert(http_response_body_handlers_append(self, "error", http_result_retry, NULL));

  const gchar *body = "{\"ok\": true} error";
  cr_assert_null(http_response_body_handlers_lookup(self, body, 12));
  cr_assert_not_null(http_response_body_handlers_lookup(self, body, strlen(body)));

  http_response_body_handlers_free(self);
}

Test(response_body_handlers, test_invalid_pattern_is_rejected)
{
  HttpResponseBodyHandlers *self = http_response_body_handlers_new();
  GError *error = NULL;

  cr_assert_not(http_response_body_handlers_append(self, "(unclosed", http_result_retry, &error));
  cr_assert_not_null(error);
  cr_assert(http_response_body_handlers_is_empty(self));

  g_clear_error(&error);
  http_response_body_handlers_free(self);
}
