/*
 * Copyright (c) 2012-2019 Balabit
 * Copyright (c) 2012-2013 Balázs Scheidler
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

#include "mock-transport.h"
#include "proto_lib.h"
#include "msg_parse_lib.h"

#include <criterion/criterion.h>

Test(log_proto, test_log_proto_server_options_limits)
{
  LogProtoServerOptions opts;

  log_proto_server_options_defaults(&opts);
  log_proto_server_options_init(&opts, configuration);
  cr_assert_gt(opts.max_msg_size, 0, "LogProtoServerOptions.max_msg_size is not initialized properly, max_msg_size=%d",
               opts.max_msg_size);
  cr_assert_gt(opts.init_buffer_size, 0,
               "LogProtoServerOptions.init_buffer_size is not initialized properly, init_buffer_size=%d", opts.init_buffer_size);
  cr_assert_gt(opts.max_buffer_size, 0,
               "LogProtoServerOptions.max_buffer_size is not initialized properly, max_buffer_size=%d", opts.max_buffer_size);
  log_proto_server_options_destroy(&opts);
}

Test(log_proto, test_log_proto_server_options_valid_encoding)
{
  LogProtoServerOptions opts;

  log_proto_server_options_defaults(&opts);
  /* check that encoding can be set and error is properly returned */
  log_proto_server_options_set_encoding(&opts, "utf-8");
  cr_assert_str_eq(opts.encoding, "utf-8", "LogProtoServerOptions.encoding was not properly set");
  log_proto_server_options_destroy(&opts);
}

Test(log_proto, test_log_proto_server_options_invalid_encoding)
{
  LogProtoServerOptions opts;
  gboolean success;

  log_proto_server_options_defaults(&opts);

  success = log_proto_server_options_set_encoding(&opts, "never-ever-is-going-to-be-such-an-encoding");
  cr_assert_str_eq(opts.encoding, "never-ever-is-going-to-be-such-an-encoding",
                   "LogProtoServerOptions.encoding was not properly set");

  log_proto_server_options_init(&opts, configuration);
  cr_assert_not(success, "Successfully set a bogus encoding, which is insane");

  log_proto_server_options_destroy(&opts);
}
