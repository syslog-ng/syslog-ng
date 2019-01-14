/*
 * Copyright (c) 2017 Balabit
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

#include <glib.h>

#include <criterion/criterion.h>
#include "plugin.h"
#include "apphook.h"
#include "syslog-names.h"
#include "stats/stats-registry.h"
#include "filter/filter-pipe.h"
#include "filter/filter-pri.h"

static gint
level_bits(gchar *lev)
{
  return 1 << syslog_name_lookup_level_by_name(lev);
}

MsgFormatOptions parse_options;

static LogFilterPipe *
create_log_filter_pipe(void)
{
  FilterExprNode *filter = filter_level_new(level_bits("debug"));
  filter_expr_init(filter, configuration);

  LogFilterPipe *p = (LogFilterPipe *)log_filter_pipe_new(filter, configuration);

  log_pipe_init(&p->super);

  return p;
}

static void
queue_and_assert_statistics(LogFilterPipe *pipe, gchar *msg, guint32 matched_expected, guint32 not_matched_expected)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *logmsg = log_msg_new(msg, strlen(msg), &parse_options);
  LogPipe *p = (LogPipe *)pipe;
  p->queue(p, logmsg, &path_options);
  cr_assert_eq(stats_counter_get(pipe->not_matched), not_matched_expected);
  cr_assert_eq(stats_counter_get(pipe->matched), matched_expected);
}

void
setup(void)
{
  app_startup();

  configuration = cfg_new_snippet();
  configuration->stats_options.level = 1;
  cfg_load_module(configuration, "syslogformat");
  cfg_init(configuration);
}

void
teardown(void)
{
  cfg_deinit(configuration);
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(test_filters_statistics, .init = setup, .fini = teardown);

Test(test_filters_statistics, filter_stastistics)
{
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);

  LogFilterPipe *p = create_log_filter_pipe();
  queue_and_assert_statistics(p, "<15> openvpn[2499]: PTHREAD support initialized", 1, 0);
  queue_and_assert_statistics(p, "<16> openvpn[2499]: PTHREAD support initialized", 1, 1);
  queue_and_assert_statistics(p, "<16> openvpn[2499]: PTHREAD support initialized", 1, 2);

  log_pipe_deinit(&p->super);
  log_pipe_unref(&p->super);
}
