/*
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Kokan <kokaipeter@gmail.com>
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

#include "groupingby.h"
#include "apphook.h"
#include "cfg.h"

static LogTemplate *
_get_template(const gchar *template, GlobalConfig *cfg)
{
  LogTemplate *self = log_template_new(cfg, "dummy");

  log_template_compile(self, template, NULL);

  return self;
}

Test(grouping_by, create_grouping_by)
{
  GlobalConfig *cfg = cfg_new_snippet();
  LogParser *parser = grouping_by_new(cfg);

  grouping_by_set_synthetic_message(parser, synthetic_message_new());

  grouping_by_set_timeout(parser, 1);

  LogTemplate *template = _get_template("$TEMPLATE", cfg);
  grouping_by_set_key_template(parser, template);
  log_template_unref(template);

  cr_assert(log_pipe_init(&parser->super));

  cr_assert(log_pipe_deinit(&parser->super));

  log_pipe_unref(&parser->super);
  cfg_free(cfg);
}

static void
setup(void)
{
  app_startup();
};

static void
teardown(void)
{
  app_shutdown();
}

TestSuite(grouping_by, .init = setup, .fini = teardown);


