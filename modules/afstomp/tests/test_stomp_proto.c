/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Viktor Tusa <tusa@balabit.hu>
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

#include "stomp.h"
#include <criterion/criterion.h>

static void
assert_stomp_header(stomp_frame *frame, char *key, char *value)
{
  char *myvalue = g_hash_table_lookup(frame->headers, key);

  cr_assert_str_eq(myvalue, value, "Stomp header assertion failed!");
}

static void
assert_stomp_command(stomp_frame *frame, char *command)
{
  cr_assert_str_eq(frame->command, command, "Stomp command assertion failed");
}

static void
assert_stomp_body(stomp_frame *frame, char *body)
{
  cr_assert_str_eq(frame->body, body, "Stomp body assertion failed");
}

Test(stomp_proto, test_only_command)
{
  stomp_frame frame;

  stomp_parse_frame(g_string_new("CONNECTED\n\n"), &frame);
  assert_stomp_command(&frame, "CONNECTED");
  stomp_frame_deinit(&frame);
}

Test(stomp_proto, test_command_and_data)
{
  stomp_frame frame;

  stomp_parse_frame(g_string_new("CONNECTED\n\nalmafa"), &frame);
  assert_stomp_command(&frame, "CONNECTED");
  assert_stomp_body(&frame, "almafa");
  stomp_frame_deinit(&frame);
};

Test(stomp_proto, test_command_and_header_and_data)
{
  stomp_frame frame;

  stomp_parse_frame(g_string_new("CONNECTED\nheader_name:header_value\n\nbelafa"), &frame);
  assert_stomp_command(&frame, "CONNECTED");
  assert_stomp_header(&frame, "header_name", "header_value");
  assert_stomp_body(&frame, "belafa");
  stomp_frame_deinit(&frame);
};

Test(stomp_proto, test_command_and_header)
{
  stomp_frame frame;

  stomp_parse_frame(g_string_new("CONNECTED\nsession:ID:tusa-38077-1378214843533-2:1\n"), &frame);
  assert_stomp_command(&frame, "CONNECTED");
  assert_stomp_header(&frame, "session", "ID:tusa-38077-1378214843533-2:1");
  stomp_frame_deinit(&frame);
};

Test(stomp_proto, test_generate_gstring_from_frame)
{
  stomp_frame frame;
  GString *actual;

  stomp_frame_init(&frame, "SEND", sizeof("SEND"));
  stomp_frame_add_header(&frame, "header_name", "header_value");
  stomp_frame_set_body(&frame, "body", sizeof("body"));
  actual = create_gstring_from_frame(&frame);
  cr_assert_str_eq(actual->str, "SEND\nheader_name:header_value\n\nbody", "Generated stomp frame does not match");
  stomp_frame_deinit(&frame);
};

Test(stomp_proto, test_invalid_command)
{
  stomp_frame frame;

  cr_assert_not(stomp_parse_frame(g_string_new("CONNECTED\n no-colon\n"), &frame));
  stomp_frame_deinit(&frame);
};
