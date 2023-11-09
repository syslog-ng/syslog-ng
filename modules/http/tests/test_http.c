/*
 * Copyright (c) 2018 Balabit
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
#include <criterion/parameterized.h>

#include "logmsg/logmsg.h"
#include "apphook.h"
#include "libtest/cr_template.h"
#include "http.h"
#include "http-worker.h"
#include "logthrdest/logthrdestdrv.h"
#include "compat/curl.h"

static void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();

  init_template_tests();
  cfg_load_module(configuration, "basicfuncs");
}

static void
teardown(void)
{
  app_shutdown();
  cfg_free(configuration);
}

TestSuite(http, .init = setup, .fini = teardown);

struct http_action_test_params
{
  glong http_code;
  char *explanation;
  LogThreadedResult expected_value;
};

ParameterizedTestParameters(http, http_code_tests)
{
  static struct http_action_test_params params[] =
  {
    { 100, "Continue", LTR_NOT_CONNECTED},
    { 101, "Switching Protocols", LTR_NOT_CONNECTED},
    { 102, "Processing", LTR_ERROR},
    { 103, "Early Hints", LTR_ERROR},

    { 200, "OK", LTR_SUCCESS},
    { 201, "Created", LTR_SUCCESS},
    { 202, "Accepted", LTR_SUCCESS},
    { 203, "Non-Authoritative Information", LTR_SUCCESS},
    { 204, "No Content", LTR_SUCCESS},
    { 205, "Reset Content", LTR_SUCCESS},
    { 206, "Partial Content", LTR_SUCCESS},

    { 300, "Multiple Choices", LTR_NOT_CONNECTED},
    { 301, "Moved Permanently", LTR_NOT_CONNECTED},
    { 302, "Found", LTR_NOT_CONNECTED},
    { 303, "See Other", LTR_NOT_CONNECTED},
    { 304, "Not Modified", LTR_ERROR},
    { 307, "Temporary Redirect", LTR_NOT_CONNECTED},
    { 308, "Permanent Redirect", LTR_NOT_CONNECTED},

    { 400, "Bad Request", LTR_NOT_CONNECTED},
    { 401, "Unauthorized", LTR_NOT_CONNECTED},
    { 402, "Payment Required", LTR_NOT_CONNECTED},
    { 403, "Forbidden", LTR_NOT_CONNECTED},
    { 404, "Not Found", LTR_NOT_CONNECTED},
    { 405, "Method Not Allowed", LTR_NOT_CONNECTED},
    { 406, "Not Acceptable", LTR_NOT_CONNECTED},
    { 407, "Proxy Authentication Required", LTR_NOT_CONNECTED},
    { 408, "Request Timeout", LTR_NOT_CONNECTED},
    { 409, "Conflict", LTR_NOT_CONNECTED},
    { 410, "Gone", LTR_DROP},
    { 411, "Length Required", LTR_NOT_CONNECTED},
    { 412, "Precondition Failed", LTR_NOT_CONNECTED},
    { 413, "Payload Too Large", LTR_NOT_CONNECTED},
    { 414, "URI Too Long", LTR_NOT_CONNECTED},
    { 415, "Unsupported Media Type", LTR_NOT_CONNECTED},
    { 416, "Range Not Satisfiable", LTR_DROP},
    { 417, "Expectation Failed", LTR_NOT_CONNECTED},
    { 418, "I'm a teapot", LTR_NOT_CONNECTED},
    { 421, "Misdirected Request", LTR_NOT_CONNECTED},
    { 422, "Unprocessable Entity", LTR_DROP},
    { 423, "Locked", LTR_NOT_CONNECTED},
    { 424, "Failed Dependency", LTR_DROP},
    { 425, "Too Early", LTR_DROP},
    { 426, "Upgrade Required", LTR_NOT_CONNECTED},
    { 428, "Precondition Required", LTR_ERROR},
    { 429, "Too Many Requests", LTR_NOT_CONNECTED},
    { 431, "Request Header Fields Too Large", LTR_NOT_CONNECTED},
    { 451, "Unavailable For Legal Reasons", LTR_DROP},

    { 500, "Internal Server Error", LTR_NOT_CONNECTED},
    { 501, "Not Implemented", LTR_NOT_CONNECTED},
    { 502, "Bad Gateway", LTR_NOT_CONNECTED},
    { 503, "Service Unavailable", LTR_NOT_CONNECTED},
    { 504, "Gateway Timeout", LTR_ERROR},
    { 505, "HTTP Version Not Supported", LTR_NOT_CONNECTED},
    { 506, "Variant Also Negotiates", LTR_NOT_CONNECTED},
    { 507, "Insufficient Storage", LTR_NOT_CONNECTED},
    { 508, "Loop Detected", LTR_DROP},
    { 510, "Not Extended", LTR_NOT_CONNECTED},
    { 511, "Network Authentication Required", LTR_NOT_CONNECTED},

    // Some external values to test default behaviour
    { 199, "doesnotexist", LTR_NOT_CONNECTED},
    { 299, "doesnotexist", LTR_SUCCESS},
    { 399, "doesnotexist", LTR_NOT_CONNECTED},
    { 399, "doesnotexist", LTR_NOT_CONNECTED},
    { 599, "doesnotexist", LTR_NOT_CONNECTED}
  };

  return cr_make_param_array(struct http_action_test_params, params, sizeof(params)/sizeof(params[0]));
}

ParameterizedTest(struct http_action_test_params *param, http, http_code_tests)
{
  HTTPDestinationDriver *driver = (HTTPDestinationDriver *) http_dd_new(configuration);
  HTTPDestinationWorker *worker = (HTTPDestinationWorker *) http_dw_new(&driver->super, 0);
  const gchar *url = "http://dummy.url";

  LogThreadedResult res =  default_map_http_status_to_worker_status(worker, url, param->http_code);
  cr_assert_eq(res, param->expected_value,
               "code: %ld, explanation: %s, actual: %s, expected: %s",
               param->http_code, param->explanation, log_threaded_result_to_str(res),
               log_threaded_result_to_str(param->expected_value));

  log_threaded_dest_worker_free(&worker->super);
  log_pipe_unref((LogPipe *)driver);
}

Test(http, set_urls)
{
  HTTPDestinationDriver *driver = (HTTPDestinationDriver *) http_dd_new(configuration);

  GList *urls = NULL;
  urls = g_list_append(urls, "http://foo.bar");
  urls = g_list_append(urls, "http://bar.baz http://almafa.kortefa");
  urls = g_list_append(urls, "http://foo.bar/${FOOBAR}");
  urls = g_list_append(urls, "http://foo.bar/$(echo ${BARBAZ})");

  GError *error = NULL;
  cr_assert(http_dd_set_urls(&driver->super.super.super, urls, &error));
  g_list_free(urls);

  HTTPLoadBalancer *lb = driver->load_balancer;
  cr_assert_eq(lb->num_targets, 5);

  cr_assert_str_eq(lb->targets[0].url_template->template_str, "http://foo.bar");
  cr_assert_str_eq(lb->targets[1].url_template->template_str, "http://bar.baz");
  cr_assert_str_eq(lb->targets[2].url_template->template_str, "http://almafa.kortefa");
  cr_assert_str_eq(lb->targets[3].url_template->template_str, "http://foo.bar/${FOOBAR}");
  cr_assert_str_eq(lb->targets[4].url_template->template_str, "http://foo.bar/$(echo ${BARBAZ})");

  log_pipe_unref(&driver->super.super.super.super);
}

#if SYSLOG_NG_CURL_FULLY_SUPPORTS_URL_PARSING
static void
_test_set_urls_fail(HTTPDestinationDriver *driver, gchar *url, const gchar *expected_error_msg)
{
  GList *urls = NULL;
  urls = g_list_append(urls, url);

  GError *error = NULL;
  cr_assert_not(http_dd_set_urls(&driver->super.super.super, urls, &error));
  g_list_free(urls);

  cr_assert_str_eq(error->message, expected_error_msg);
  g_error_free(error);
}

static void
_test_set_urls_success(HTTPDestinationDriver *driver, gchar *url)
{
  GList *urls = NULL;
  urls = g_list_append(urls, url);

  GError *error = NULL;
  cr_assert(http_dd_set_urls(&driver->super.super.super, urls, &error));

  g_list_free(urls);
}

Test(http, set_urls_safe_or_unsafe_template)
{
  HTTPDestinationDriver *driver = (HTTPDestinationDriver *) http_dd_new(configuration);

  _test_set_urls_fail(driver, "$(echo $foo)",
                      "Scheme part of URL cannot contain templates: $(echo $foo)");
  _test_set_urls_fail(driver, "$foo",
                      "Scheme part of URL cannot contain templates: $foo");

  _test_set_urls_fail(driver, "http://$(echo $foo)",
                      "Host part of URL cannot contain templates: http://$(echo $foo)");
  _test_set_urls_fail(driver, "http://$foo",
                      "Host part of URL cannot contain templates: http://$foo");

  _test_set_urls_fail(driver, "$(echo $foo)://bar.baz",
                      "Scheme part of URL cannot contain templates: $(echo $foo)://bar.baz");
  _test_set_urls_fail(driver, "$foo://bar.baz",
                      "Scheme part of URL cannot contain templates: $foo://bar.baz");

  _test_set_urls_fail(driver, "http://bar.baz:$(echo $foo)",
                      "Port part of URL cannot contain templates: http://bar.baz:$(echo $foo)");
  _test_set_urls_fail(driver, "http://bar.baz:$foo",
                      "Port part of URL cannot contain templates: http://bar.baz:$foo");

  _test_set_urls_fail(driver, "http://$(echo $foo):almafa@bar.baz",
                      "User part of URL cannot contain templates: http://$(echo $foo):almafa@bar.baz");
  _test_set_urls_fail(driver, "http://$foo:almafa@bar.baz",
                      "User part of URL cannot contain templates: http://$foo:almafa@bar.baz");

  _test_set_urls_fail(driver, "http://almafa:$(echo $foo)@bar.baz",
                      "Password part of URL cannot contain templates: http://almafa:$(echo $foo)@bar.baz");
  _test_set_urls_fail(driver, "http://almafa:$foo@bar.baz",
                      "Password part of URL cannot contain templates: http://almafa:$foo@bar.baz");

  _test_set_urls_success(driver, "http://bar.baz/$(echo $foo)");
  _test_set_urls_success(driver, "http://bar.baz/$foo");

  _test_set_urls_success(driver, "http://bar.baz?$(echo $foo)");
  _test_set_urls_success(driver, "http://bar.baz?$foo");

  log_pipe_unref(&driver->super.super.super.super);
}
#endif
