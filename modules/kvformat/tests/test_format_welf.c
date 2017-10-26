/*
 * Copyright (c) 2011-2013 Balabit
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
  assert_template_format("$(format-welf MSG=$escaping)",
                         "MSG=\"binary stuff follows \\\"\\xad árvíztűrőtükörfúrógép\"");
  assert_template_format("$(format-welf MSG=$escaping2)", "MSG=\\xc3");
  assert_template_format("$(format-welf MSG=$null)", "MSG=binary\\x00stuff");
  assert_template_format_with_context("$(format-welf MSG=$MSG)",
                                      "MSG=árvíztűrőtükörfúrógép MSG=árvíztűrőtükörfúrógép");
}

void
test_format_welf_performance(void)
{
  perftest_template("$(format-welf APP.*)\n");
  perftest_template("<$PRI>1 $ISODATE $LOGHOST @syslog-ng - - ${SDATA:--} $(format-welf --scope all-nv-pairs "
                    "--exclude 0* --exclude 1* --exclude 2* --exclude 3* --exclude 4* --exclude 5* "
                    "--exclude 6* --exclude 7* --exclude 8* --exclude 9* "
                    "--exclude SOURCE "
                    "--exclude .SDATA.* "
                    "..RSTAMP='${R_UNIXTIME}${R_TZ}' "
                    "..TAGS=${TAGS})\n");
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();
  putenv("TZ=UTC");
  tzset();
  init_template_tests();
  cfg_load_module(configuration, "kvformat");

  test_format_welf();
  test_format_welf_performance();

  deinit_template_tests();
  app_shutdown();
}
