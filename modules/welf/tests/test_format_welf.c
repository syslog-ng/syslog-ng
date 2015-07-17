/*
 * Copyright (c) 2011-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2011-2013 Gergely Nagy <algernon@balabit.hu>
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
#include "template_lib.h"
#include "apphook.h"
#include "plugin.h"
#include "cfg.h"

void
test_format_welf(void)
{
  assert_template_format("$(format-welf MSG=$MSG)", "MSG=árvíztűrőtükörfúrógép");
  assert_template_format("$(format-welf MSG=$escaping)", "MSG=\"binary stuff follows \\\"\\xad árvíztűrőtükörfúrógép\"");
  assert_template_format_with_context("$(format-welf MSG=$MSG)", "MSG=árvíztűrőtükörfúrógép MSG=árvíztűrőtükörfúrógép");
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();
  putenv("TZ=UTC");
  tzset();
  init_template_tests();
  plugin_load_module("welf", configuration, NULL);

  test_format_welf();

  deinit_template_tests();
  app_shutdown();
}
