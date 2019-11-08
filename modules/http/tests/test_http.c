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

#include <syslog-ng.h>
#include <logmsg/logmsg.h>
#include <apphook.h>
#include "http.h"
#include "http-worker.h"
#include "logthrdest/logthrdestdrv.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

TestSuite(http, .init = app_startup, .fini = app_shutdown);

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
    { 200, "filled later", LTR_SUCCESS},
    { 301, "filled later", LTR_ERROR},
    { 404, "filled later", LTR_DROP},
    { 500, "filled later", LTR_ERROR}
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
