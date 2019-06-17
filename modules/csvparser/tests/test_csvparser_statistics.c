/*
 * Copyright (c) 2008-2017 Balabit
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

#include "csvparser.h"
#include "syslog-ng.h"
#include "logmsg/logmsg.h"
#include "plugin.h"
#include "string-list.h"
#include "apphook.h"
#include "cfg.h"

#include <criterion/criterion.h>

LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

static LogParser *
_create_parser(GlobalConfig *cfg)
{
  LogParser *p = csv_parser_new(cfg);
  const gchar *column_array[] = { "header1", NULL };

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_PARSER, p->name, NULL );
  stats_register_counter(1, &sc_key, SC_TYPE_DISCARDED, &p->super.discarded_messages);
  stats_unlock();

  csv_scanner_options_set_delimiters(csv_parser_get_scanner_options(p), ",");
  csv_scanner_options_set_columns(csv_parser_get_scanner_options(p), string_array_to_list(column_array));
  csv_parser_set_drop_invalid(p, TRUE);

  return p;
}

static void
_unregister_statistics(LogParser *p)
{
  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_PARSER, p->name, NULL );
  stats_unregister_counter(&sc_key, SC_TYPE_DISCARDED, &p->super.discarded_messages);
  stats_unlock();

}

static void
_parse_msg(LogParser *self, gchar *msg)
{
  LogMessage *logmsg = log_msg_new_empty();
  log_msg_set_value(logmsg, LM_V_MESSAGE, msg, -1);
  log_parser_process_message(self, &logmsg, &path_options);
  log_msg_unref(logmsg);
}

Test(test_filters_statistics, filter_statistics)
{
  app_startup();
  configuration = cfg_new_snippet();
  configuration->stats_options.level = 1;
  cr_assert(cfg_init(configuration));

  LogParser *parser = _create_parser(configuration);
  cr_assert_eq(stats_counter_get(parser->super.discarded_messages), 0);
  _parse_msg(parser, "column1, column2, column3");
  cr_assert_eq(stats_counter_get(parser->super.discarded_messages), 1);
  _parse_msg(parser, "column1, column2");
  cr_assert_eq(stats_counter_get(parser->super.discarded_messages), 2);
  _parse_msg(parser, "column1");
  cr_assert_eq(stats_counter_get(parser->super.discarded_messages), 2);

  _unregister_statistics(parser);

  log_pipe_unref(&parser->super);
  cfg_deinit(configuration);
  cfg_free(configuration);
  app_shutdown();
}
