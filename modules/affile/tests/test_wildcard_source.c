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
 *
 */

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include "libtest/config_parse_lib.h"
#include "libtest/grab-logging.h"

#include "cfg.h"
#include "apphook.h"
#include "cfg-grammar.h"
#include "plugin.h"
#include "wildcard-source.h"

/* Cases: directory modification notifications wake effective inotify readers,
 * including auto/legacy selection, but do not wake timer/system-polled readers.
 */
#if SYSLOG_NG_HAVE_INOTIFY
typedef struct
{
  FollowMethod requested;
  FollowMethod effective;
  gboolean expect_check;
} ModifiedEventTestCase;

ParameterizedTestParameters(wildcard_source, modified_event_uses_effective_follow_mode)
{
  static ModifiedEventTestCase cases[] =
  {
    { FM_AUTO, FM_INOTIFY, TRUE },
    { FM_LEGACY, FM_INOTIFY, TRUE },
    { FM_INOTIFY, FM_INOTIFY, TRUE },
    { FM_POLL, FM_POLL, FALSE },
    { FM_SYSTEM_POLL, FM_SYSTEM_POLL, FALSE },
  };
  return cr_make_param_array(ModifiedEventTestCase, cases, G_N_ELEMENTS(cases));
}

ParameterizedTest(ModifiedEventTestCase *test_case, wildcard_source, modified_event_uses_effective_follow_mode)
{
  FileReaderOptions options = { .follow_method = test_case->requested };
  LogReader *reader = log_reader_new(configuration);
  reader->poll_events = g_new0(PollEvents, 1);
  poll_events_init(reader->poll_events);
  reader->poll_events->type = test_case->effective;
  reader->watches_running = TRUE;
  FileReader file_reader = { .reader = reader, .options = &options };

  file_reader_notify_method(&file_reader.super, NC_FILE_MODIFIED, NULL);
  cr_assert_eq(!!iv_task_registered(&reader->restart_task), test_case->expect_check);

  if (iv_task_registered(&reader->restart_task))
    iv_task_unregister(&reader->restart_task);
  reader->watches_running = FALSE;
  log_pipe_unref(&reader->super.super);
}
#endif

static void
_init(void)
{
  app_startup();
  configuration = cfg_new_snippet();
  cr_assert(cfg_load_module(configuration, "affile"));
}

static void
_deinit(void)
{
  cfg_deinit(configuration);
  cfg_free(configuration);
}

static gboolean
_parse_config(const gchar *config)
{
  gchar *raw_config = g_strdup_printf("source s_test { wildcard-file(%s); }; log { source(s_test); };", config);
  gboolean result = parse_config(raw_config, LL_CONTEXT_ROOT, NULL, NULL);
  g_free(raw_config);
  return result;
}

static WildcardSourceDriver *
_create_wildcard_filesource(const gchar *wildcard_config)
{
  cr_assert(_parse_config(wildcard_config), "Parsing the given configuration failed");
  cr_assert(cfg_init(configuration), "Config initialization failed");
  LogExprNode *expr_node = cfg_tree_get_object(&configuration->tree, ENC_SOURCE, "s_test");
  cr_assert(expr_node != NULL);
  WildcardSourceDriver *driver = (WildcardSourceDriver *)expr_node->children->children->object;
  cr_assert(driver != NULL);
  return driver;
}

TestSuite(wildcard_source, .init = _init, .fini = _deinit);

Test(wildcard_source, initial_test)
{
  WildcardSourceDriver *driver = _create_wildcard_filesource("base-dir(/test_non_existent_dir)"
                                                             "filename-pattern(*.log)"
                                                             "recursive(yes)"
                                                             "max-files(100)"
                                                             "monitor-method(poll)");
  cr_assert_str_eq(driver->base_dir, "/test_non_existent_dir");
  cr_assert_str_eq(driver->filename_pattern, "*.log");
  cr_assert_eq(driver->max_files, 100);
  cr_assert_eq(driver->recursive, TRUE);
  cr_assert_eq(driver->monitor_method, MM_POLL);
}

Test(wildcard_source, test_option_inheritance_multiline)
{
  WildcardSourceDriver *driver = _create_wildcard_filesource("base-dir(/test_non_existent_dir)"
                                                             "filename-pattern(*.log)"
                                                             "recursive(yes)"
                                                             "max-files(100)"
                                                             "follow-freq(10)"
                                                             "multi-line-mode(regexp)"
                                                             "multi-line-prefix('\\d+')"
                                                             "multi-line-garbage(garbage)");
  cr_assert_eq(driver->file_reader_options.follow_freq, 10000);
  cr_assert_eq(file_reader_options_get_log_proto_options(
                 &driver->file_reader_options)->super.super.multi_line_options.mode,
               MLM_REGEXP_PREFIX_GARBAGE);
  cr_assert(file_reader_options_get_log_proto_options(
              &driver->file_reader_options)->super.super.multi_line_options.regexp.prefix !=
            NULL);
  cr_assert(file_reader_options_get_log_proto_options(
              &driver->file_reader_options)->super.super.multi_line_options.regexp.garbage !=
            NULL);
}

Test(wildcard_source, test_option_inheritance_padded)
{
  WildcardSourceDriver *driver = _create_wildcard_filesource("base-dir(/test_non_existent_dir)"
                                                             "filename-pattern(*.log)"
                                                             "recursive(yes)"
                                                             "max-files(100)"
                                                             "pad-size(5)");
  cr_assert_eq(file_reader_options_get_log_proto_options(&driver->file_reader_options)->super.pad_size, 5);
}

Test(wildcard_source, test_option_duplication)
{
  WildcardSourceDriver *driver = _create_wildcard_filesource("base-dir(/tmp)"
                                                             "filename-pattern(*.txt)"
                                                             "follow-freq(10)"
                                                             "base-dir(/test_non_existent_dir)"
                                                             "filename-pattern(*.log)"
                                                             "follow-freq(10.5)"
                                                            );
  cr_assert_str_eq(driver->base_dir, "/test_non_existent_dir");
  cr_assert_str_eq(driver->filename_pattern, "*.log");
  cr_assert_eq(driver->file_reader_options.follow_freq, 10500);
}

Test(wildcard_source, test_filename_pattern_required_options)
{
  start_grabbing_messages();
  cr_assert(_parse_config("base-dir(/tmp)"));
  cr_assert(!cfg_init(configuration), "Config initialization should be failed");
  stop_grabbing_messages();
  assert_grabbed_log_contains("filename-pattern() option is required");
  reset_grabbed_messages();
}

Test(wildcard_source, test_base_dir_required_options)
{
  start_grabbing_messages();
  cr_assert(_parse_config("filename-pattern(/tmp)"));
  cr_assert(!cfg_init(configuration), "Config initialization should be failed");
  stop_grabbing_messages();
  assert_grabbed_log_contains("base-dir() option is required");
  reset_grabbed_messages();
}

Test(wildcard_source, test_invalid_monitor_method)
{
  start_grabbing_messages();
  cr_assert(!_parse_config("monitor-method(\"something else\""));
  stop_grabbing_messages();
  assert_grabbed_log_contains("Invalid value for monitor-method()");
  reset_grabbed_messages();
}

Test(wildcard_source, test_minimum_window_size)
{
  WildcardSourceDriver *driver = _create_wildcard_filesource("base-dir(/test_non_existent_dir)"
                                                             "filename-pattern(*.log)"
                                                             "recursive(yes)"
                                                             "max_files(100)"
                                                             "log_iw_size(1000)");
  cr_assert_eq(driver->file_reader_options.reader_options.super.init_window_size, 100);
}

Test(wildcard_source, test_window_size)
{

  WildcardSourceDriver *driver = _create_wildcard_filesource("base-dir(/test_non_existent_dir)"
                                                             "filename-pattern(*.log)"
                                                             "recursive(yes)"
                                                             "max_files(10)"
                                                             "log_iw_size(10000)");
  cr_assert_eq(driver->file_reader_options.reader_options.super.init_window_size, 1000);
}


/*
 * Criterion parameter payloads must be self-contained here.
 * We use fixed-size arrays (not pointers) to avoid pointer invalidation across
 * worker process boundaries on macOS
 */
struct LegacyWildcardTestParams
{
  gchar path[64];
  gchar expected_base_dir[64];
  gchar expected_filename_pattern[64];
};

ParameterizedTestParameters(wildcard_source, test_legacy_wildcard)
{
  static struct LegacyWildcardTestParams params[] =
  {
    { "/a/b/c/d*", "/a/b/c", "d*" },
    { "/a/b/c/d?", "/a/b/c", "d?" },
    { "/*", "/", "*" },
    { "*", ".", "*" },
    { "/tmp/*", "/tmp", "*" },
    { "tmp/?", "tmp", "?" },
    { "tmp*", ".", "tmp*" },
    { "/tmp*", "/", "tmp*" },
    { "tmp/a*", "tmp", "a*" },
  };


  return cr_make_param_array(struct LegacyWildcardTestParams, params, G_N_ELEMENTS(params));
}

ParameterizedTest(struct LegacyWildcardTestParams *params, wildcard_source, test_legacy_wildcard)
{
  WildcardSourceDriver *driver = (WildcardSourceDriver *) wildcard_sd_legacy_new(params->path, configuration);

  cr_assert_str_eq(driver->base_dir, params->expected_base_dir);
  cr_assert_str_eq(driver->filename_pattern, params->expected_filename_pattern);

  log_pipe_unref(&driver->super.super.super);
}
