/*
 * Copyright (c) 2022 One Identity
 * Copyright (c) 2022 László Várady
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

#include "apphook.h"
#include "cfg.h"
#include "syslog-format.h"
#include "logmsg/logmsg.h"
#include "msg-format.h"
#include "scratch-buffers.h"

#include <string.h>

GlobalConfig *cfg;
MsgFormatOptions parse_options;

static void
setup(void)
{
  app_startup();
  syslog_format_init();

  cfg = cfg_new_snippet();
  msg_format_options_defaults(&parse_options);
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
  cfg_free(cfg);
}

TestSuite(syslog_format, .init = setup, .fini = teardown);

Test(syslog_format, parser_should_not_spin_on_non_zero_terminated_input, .timeout = 10)
{
  const gchar *data = "<182>2022-08-17T05:02:28.217 mymachine su: 'su root' failed for lonvick on /dev/pts/8";
  /* chosen carefully to reproduce a bug */
  gsize data_length = 27;

  msg_format_options_init(&parse_options, cfg);
  LogMessage *msg = msg_format_construct_message(&parse_options, (const guchar *) data, data_length);

  gsize problem_position;
  cr_assert(syslog_format_handler(&parse_options, msg, (const guchar *) data, data_length, &problem_position));

  msg_format_options_destroy(&parse_options);
  log_msg_unref(msg);
}
