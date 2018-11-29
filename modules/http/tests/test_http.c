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
#include "logthrdestdrv.h"

#include <criterion/criterion.h>

TestSuite(http, .init = app_startup, .fini = app_shutdown);

Test(http, test_error_codes)
{
  HTTPDestinationDriver *driver = (HTTPDestinationDriver *) http_dd_new(configuration);
  HTTPDestinationWorker *worker = (HTTPDestinationWorker *) http_dw_new(&driver->super, 0);
  const gchar *url = "http://dummy.url";

  cr_assert_eq(map_http_status_to_worker_status(worker, url, 200), WORKER_INSERT_RESULT_SUCCESS);
  cr_assert_eq(map_http_status_to_worker_status(worker, url, 301), WORKER_INSERT_RESULT_ERROR);
  cr_assert_eq(map_http_status_to_worker_status(worker, url, 404), WORKER_INSERT_RESULT_DROP);
  cr_assert_eq(map_http_status_to_worker_status(worker, url, 500), WORKER_INSERT_RESULT_ERROR);

  log_threaded_dest_worker_free(&worker->super);
  log_pipe_unref((LogPipe *)driver);
}
