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

#include "messages.h"
#include "apphook.h"
#include "logmsg/logmsg.h"

#include <criterion/criterion.h>

TestSuite(test_messages, .init = app_startup, .fini = app_shutdown);

void simple_test_asserter(LogMessage *msg)
{
  cr_assert_str_eq(log_msg_get_value_by_name(msg, "MESSAGE", NULL), "simple test;");
  log_msg_unref(msg);
}

Test(test_messages, simple_test)
{
  msg_set_post_func(simple_test_asserter);
  msg_error("simple test");
}
