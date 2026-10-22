/*
 * Copyright (c) 2017-2019 Balabit
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
#include "libtest/msg_parse_lib.h"

#include "geoip-parser.h"
#include "maxminddb-helper.h"
#include "apphook.h"
#include "scratch-buffers.h"

GlobalConfig *cfg;
LogParser *geoip_parser;

void
setup(void)
{
  app_startup();
  cfg = cfg_new_snippet();
  geoip_parser = maxminddb_parser_new(cfg);
  /*
   * The origin of the database:
   * https://github.com/maxmind/MaxMind-DB/blob/ea7cd314bb55879b8cb1a059c425d53ff3b9b6cc/test-data/GeoIP2-Precision-Enterprise-Test.mmdb
   * The same in JSON format:
   * https://github.com/maxmind/MaxMind-DB/blob/ea7cd314bb55879b8cb1a059c425d53ff3b9b6cc/source-data/GeoIP2-Precision-Enterprise-Test.json
   */
  geoip_parser_set_database_path(geoip_parser, TOP_SRCDIR "/modules/geoip2/tests/test.mmdb");
}

void
teardown(void)
{
  log_pipe_deinit(&geoip_parser->super);
  log_pipe_unref(&geoip_parser->super);
  scratch_buffers_explicit_gc();
  cfg_free(cfg);
  app_shutdown();
}

static LogMessage *
parse_geoip_into_log_message_no_check(const gchar *template_format)
{
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogParser *cloned_parser;
  gboolean success;

  cloned_parser = (LogParser *) log_pipe_clone(&geoip_parser->super);

  LogTemplate *template = log_template_new(NULL, NULL);
  cr_assert(log_template_compile(template, template_format, NULL));
  log_parser_set_template(cloned_parser, template);

  log_pipe_init(&cloned_parser->super);
  msg = log_msg_new_empty();

  success = log_parser_process_message(cloned_parser, &msg, &path_options);
  if (!success)
    {
      log_msg_unref(msg);
      msg = NULL;
    }
  log_pipe_deinit(&cloned_parser->super);
  log_pipe_unref(&cloned_parser->super);
  return msg;
}

static LogMessage *
parse_geoip_into_log_message(const gchar *template_format)
{
  LogMessage *msg;

  msg = parse_geoip_into_log_message_no_check(template_format);
  cr_assert_not_null(msg, "expected geoip-parser success and it returned failure, template_format=%s", template_format);
  return msg;
}

Test(geoip2, template_is_mandatory)
{
  LogParser *geoip2_parser = maxminddb_parser_new(configuration);

  cr_assert_not(log_pipe_init(&geoip2_parser->super));

  log_pipe_unref(&geoip2_parser->super);
}

Test(geoip2, set_prefix)
{
  LogMessage *msg;

  geoip_parser_set_prefix(geoip_parser, ".prefix.");
  msg = parse_geoip_into_log_message("2.125.160.216");
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.country.iso_code"), "GB");
  log_msg_unref(msg);
}

Test(geoip2, empty_prefix)
{
  LogMessage *msg;

  geoip_parser_set_prefix(geoip_parser, "");
  msg = parse_geoip_into_log_message("2.125.160.216");
  assert_log_message_value(msg, log_msg_get_value_handle(".country.iso_code"), "GB");
  log_msg_unref(msg);
}

Test(geoip2, test_basic)
{
  LogMessage *msg;

  msg = parse_geoip_into_log_message("2.125.160.216");
  assert_log_message_value(msg, log_msg_get_value_handle(".geoip2.country.iso_code"), "GB");

  assert_log_message_value(msg, log_msg_get_value_handle(".geoip2.location.latitude"), "51.750000");
  assert_log_message_value(msg, log_msg_get_value_handle(".geoip2.location.longitude"), "-1.250000");

  log_msg_unref(msg);
}

Test(geoip2, missing_entry_preserves_message)
{
  LogMessage *msg = parse_geoip_into_log_message("192.0.2.10");
  assert_log_message_value(msg, log_msg_get_value_handle(".geoip2.country.iso_code"), "");
  log_msg_unref(msg);
}

Test(geoip2, invalid_address_preserves_message)
{
  LogMessage *msg = parse_geoip_into_log_message("not-an-ip-address");
  assert_log_message_value(msg, log_msg_get_value_handle(".geoip2.country.iso_code"), "");
  log_msg_unref(msg);
}

static MMDB_entry_data_list_s
_mmdb_string(const gchar *value)
{
  MMDB_entry_data_list_s entry = {0};
  entry.entry_data.type = MMDB_DATA_TYPE_UTF8_STRING;
  entry.entry_data.utf8_string = value;
  entry.entry_data.data_size = strlen(value);
  return entry;
}

static MMDB_entry_data_list_s
_mmdb_container(guint32 type, guint32 size)
{
  MMDB_entry_data_list_s entry = {0};
  entry.entry_data.type = type;
  entry.entry_data.data_size = size;
  return entry;
}

static LogMessage *
_assert_walk_status(MMDB_entry_data_list_s *entries, gsize count, gint expected_status)
{
  for (gsize i = 0; i + 1 < count; i++)
    entries[i].next = &entries[i + 1];

  GArray *path = g_array_new(TRUE, FALSE, sizeof(gchar *));
  gchar *prefix = ".geoip2";
  g_array_append_val(path, prefix);
  LogMessage *msg = log_msg_new_empty();

  /* Neither successful nor failed walks may depend on the incoming status. */
  gint status = -1;
  MMDB_entry_data_list_s *next = dump_geodata_into_msg(msg, entries, path, &status);
  cr_assert_eq(status, expected_status);
  cr_assert_null(next);
  if (status == MMDB_SUCCESS)
    cr_assert_eq(path->len, 1);

  g_array_free(path, TRUE);
  return msg;
}

Test(geoip2, scalar_walk_sets_status)
{
  MMDB_entry_data_list_s entry = _mmdb_string("value");
  LogMessage *msg = _assert_walk_status(&entry, 1, MMDB_SUCCESS);
  assert_log_message_value(msg, log_msg_get_value_handle(".geoip2"), "value");
  log_msg_unref(msg);
}

Test(geoip2, successful_walk_returns_next_node)
{
  MMDB_entry_data_list_s entries[] = { _mmdb_string("first"), _mmdb_string("second") };
  entries[0].next = &entries[1];
  GArray *path = g_array_new(TRUE, FALSE, sizeof(gchar *));
  gchar *prefix = ".geoip2";
  g_array_append_val(path, prefix);
  LogMessage *msg = log_msg_new_empty();
  gint status = -1;

  cr_assert_eq(dump_geodata_into_msg(msg, entries, path, &status), &entries[1]);
  cr_assert_eq(status, MMDB_SUCCESS);
  assert_log_message_value(msg, log_msg_get_value_handle(".geoip2"), "first");

  g_array_free(path, TRUE);
  log_msg_unref(msg);
}

Test(geoip2, empty_container_walk_sets_status)
{
  const guint32 types[] = { MMDB_DATA_TYPE_MAP, MMDB_DATA_TYPE_ARRAY };
  for (gsize i = 0; i < G_N_ELEMENTS(types); i++)
    {
      MMDB_entry_data_list_s entry = _mmdb_container(types[i], 0);
      log_msg_unref(_assert_walk_status(&entry, 1, MMDB_SUCCESS));
    }
}

Test(geoip2, preferred_language_first_sets_status_and_continues_walk)
{
  MMDB_entry_data_list_s entries[] =
  {
    _mmdb_container(MMDB_DATA_TYPE_MAP, 2),
    _mmdb_string("names"),
    _mmdb_container(MMDB_DATA_TYPE_MAP, 1),
    _mmdb_string("en"),
    _mmdb_string("Boxford"),
    _mmdb_string("country"),
    _mmdb_container(MMDB_DATA_TYPE_MAP, 1),
    _mmdb_string("iso_code"),
    _mmdb_string("GB"),
  };
  LogMessage *msg = _assert_walk_status(entries, G_N_ELEMENTS(entries), MMDB_SUCCESS);
  assert_log_message_value(msg, log_msg_get_value_handle(".geoip2.names.en"), "Boxford");
  assert_log_message_value(msg, log_msg_get_value_handle(".geoip2.country.iso_code"), "GB");
  log_msg_unref(msg);
}

Test(geoip2, empty_names_map_sets_status)
{
  MMDB_entry_data_list_s entries[] =
  {
    _mmdb_container(MMDB_DATA_TYPE_MAP, 1),
    _mmdb_string("names"),
    _mmdb_container(MMDB_DATA_TYPE_MAP, 0),
  };
  log_msg_unref(_assert_walk_status(entries, G_N_ELEMENTS(entries), MMDB_SUCCESS));
}

Test(geoip2, skipped_empty_container_sets_status)
{
  const guint32 types[] = { MMDB_DATA_TYPE_MAP, MMDB_DATA_TYPE_ARRAY };
  for (gsize i = 0; i < G_N_ELEMENTS(types); i++)
    {
      MMDB_entry_data_list_s entries[] =
      {
        _mmdb_container(MMDB_DATA_TYPE_MAP, 1),
        _mmdb_string("names"),
        _mmdb_container(MMDB_DATA_TYPE_MAP, 1),
        _mmdb_string("de"),
        _mmdb_container(types[i], 0),
      };
      log_msg_unref(_assert_walk_status(entries, G_N_ELEMENTS(entries), MMDB_SUCCESS));
    }
}

Test(geoip2, nested_array_walk_sets_status)
{
  MMDB_entry_data_list_s entries[] =
  {
    _mmdb_container(MMDB_DATA_TYPE_ARRAY, 1),
    _mmdb_container(MMDB_DATA_TYPE_MAP, 1),
    _mmdb_string("names"),
    _mmdb_container(MMDB_DATA_TYPE_MAP, 1),
    _mmdb_string("en"),
    _mmdb_string("Boxford"),
  };
  LogMessage *msg = _assert_walk_status(entries, G_N_ELEMENTS(entries), MMDB_SUCCESS);
  assert_log_message_value(msg, log_msg_get_value_handle(".geoip2.0.names.en"), "Boxford");
  log_msg_unref(msg);
}

Test(geoip2, invalid_preferred_language_value_sets_error)
{
  MMDB_entry_data_list_s entries[] =
  {
    _mmdb_container(MMDB_DATA_TYPE_MAP, 1),
    _mmdb_string("names"),
    _mmdb_container(MMDB_DATA_TYPE_MAP, 1),
    _mmdb_string("en"),
    _mmdb_container(MMDB_DATA_TYPE_MAP, 0),
  };
  log_msg_unref(_assert_walk_status(entries, G_N_ELEMENTS(entries), MMDB_INVALID_DATA_ERROR));
}

Test(geoip2, skipped_nested_map_error_is_preserved)
{
  MMDB_entry_data_list_s entries[] =
  {
    _mmdb_container(MMDB_DATA_TYPE_MAP, 1),
    _mmdb_string("names"),
    _mmdb_container(MMDB_DATA_TYPE_MAP, 1),
    _mmdb_string("de"),
    _mmdb_container(MMDB_DATA_TYPE_ARRAY, 1),
    _mmdb_container(MMDB_DATA_TYPE_MAP, 1),
    /* A map key must be a string, even inside a skipped language subtree. */
    _mmdb_container(MMDB_DATA_TYPE_ARRAY, 0),
  };
  log_msg_unref(_assert_walk_status(entries, G_N_ELEMENTS(entries), MMDB_INVALID_DATA_ERROR));
}

TestSuite(geoip2, .init = setup, .fini = teardown);
