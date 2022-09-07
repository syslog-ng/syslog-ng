/*
 * Copyright (c) 2022 One Identity
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

#include <criterion/criterion.h>
#include <evtlog.h>
#include <criterion/parameterized.h>

struct evt_tag_mem_params
{
  gchar *original_value;
  gsize original_value_len;
  gchar *new_value;
};

ParameterizedTestParameters(evt_tag_mem, test)
{
  static struct evt_tag_mem_params params[] =
  {
    {"", 1, "."},
    {"\0", 2, ".."},
    {"foo", 4, "foo."},
    {"\0\0foo", 6, "..foo."},
    {"\0a\0b\0c", 7, ".a.b.c."},
  };

  return cr_make_param_array(struct evt_tag_mem_params, params, sizeof (params) / sizeof(struct evt_tag_mem_params));
}

ParameterizedTest(struct evt_tag_mem_params *params, evt_tag_mem, test)
{
  gchar *key = "test:evt_tag_mem";
  EVTCONTEXT *ctx = evt_ctx_init("evt_tag_mem", LOG_AUTH);
  EVTREC *event_rec = evt_rec_init(ctx, LOG_INFO, "");

  evt_rec_add_tag(event_rec, evt_tag_mem(key, params->original_value, params->original_value_len));
  gchar *formatted_result = evt_format(event_rec);

  GString *expected_result = g_string_sized_new(8);
  g_string_append_printf(expected_result, "; %s='%s'", key, params->new_value);

  cr_assert_str_eq(formatted_result, expected_result->str);


  free(formatted_result);
  g_string_free(expected_result, TRUE);
  evt_ctx_free(ctx);
}
