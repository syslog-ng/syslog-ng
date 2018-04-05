/*
 * Copyright (c) 2017 Balabit
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
 */

#include <criterion/criterion.h>

#include "libtest/cr_template.h"
#include "apphook.h"
#include "cfg.h"

void
setup(void)
{
  app_startup();
  setenv("TZ", "UTC", TRUE);
  tzset();
  init_template_tests();
  cfg_load_module(configuration, "graphite");
}

void
teardown(void)
{
  deinit_template_tests();
  app_shutdown();
}

TestSuite(graphite_output, .init = setup, .fini = teardown);

Test(graphite_output, test_graphite_plaintext_proto_simple)
{
  assert_template_format("$(graphite-output local.random.diceroll=4)", "local.random.diceroll 4 1139684315\n");
}

Test(graphite_output, test_graphite_output_key)
{
  assert_template_format("$(graphite-output --key MESSAGE)", "MESSAGE árvíztűrőtükörfúrógép 1139684315\n");
  assert_template_format("$(graphite-output --key APP.VALUE*)",
                         "APP.VALUE value 1139684315\n"
                         "APP.VALUE2 value 1139684315\n"
                         "APP.VALUE3 value 1139684315\n"
                         "APP.VALUE4 value 1139684315\n"
                         "APP.VALUE5 value 1139684315\n"
                         "APP.VALUE6 value 1139684315\n"
                         "APP.VALUE7 value 1139684315\n");
  assert_template_format("$(graphite-output local.value=${APP.VALUE})", "local.value value 1139684315\n");
}

Test(graphite_output, test_graphite_output_timestamp)
{
  assert_template_format("$(graphite-output --timestamp 123 x=y)", "x y 123\n");
}
