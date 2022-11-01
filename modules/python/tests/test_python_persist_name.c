/*
 * Copyright (c) 2019 Balabit
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

/* this has to come first for modules which include the Python.h header */
#include "python-module.h"

#include <criterion/criterion.h>
#include "libtest/grab-logging.h"

#include "python-helpers.h"
#include "apphook.h"
#include "python-dest.h"
#include "python-main.h"
#include "python-fetcher.h"
#include "python-source.h"
#include "python-bookmark.h"
#include "python-ack-tracker.h"
#include "mainloop-worker.h"
#include "mainloop.h"
#include "python-persist.h"


MainLoop *main_loop;
MainLoopOptions main_loop_options = {0};

static PyObject *_python_main;
static PyObject *_python_main_dict;

CFG_LTYPE yyltype;
GlobalConfig *empty_cfg;

static void
_py_init_interpreter(void)
{
  Py_Initialize();
  py_init_argv();

  py_init_threads();
  py_log_fetcher_global_init();
  py_log_source_global_init();
  py_bookmark_global_init();
  py_log_destination_global_init();
  py_ack_tracker_global_init();
  PyEval_SaveThread();
}

static void
_init_python_main(void)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  {
    _python_main = PyImport_AddModule("__main__");
    _python_main_dict = PyModule_GetDict(_python_main);
  }
  PyGILState_Release(gstate);
}

void setup(void)
{
  app_startup();

  main_loop = main_loop_get_instance();
  main_loop_init(main_loop, &main_loop_options);

  _py_init_interpreter();
  _init_python_main();

  empty_cfg = cfg_new_snippet();
}

void teardown(void)
{
  cfg_free(empty_cfg);
  main_loop_deinit(main_loop);
  app_shutdown();
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  Py_DECREF(_python_main);
  Py_DECREF(_python_main_dict);
  PyGILState_Release(gstate);
}

static void
_load_code(const gchar *code)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  cr_assert(python_evaluate_global_code(empty_cfg, code, &yyltype));
  PyGILState_Release(gstate);
}

TestSuite(python_persist_name, .init = setup, .fini = teardown);

const gchar *python_destination_code = "\n\
from _syslogng import LogDestination\n\
class Dest(LogDestination):\n\
    @staticmethod\n\
    def generate_persist_name(options):\n\
        return options['key']\n\
    def send(self, message):\n\
        return True";

Test(python_persist_name, test_python_dest)
{
  _load_code(python_destination_code);

  LogDriver *d = python_dd_new(empty_cfg);
  python_dd_set_class(d, "Dest");
  python_dd_set_option(d, "key", "value");
  cr_assert(log_pipe_init((LogPipe *)d));

  cr_assert_str_eq(log_pipe_get_persist_name((LogPipe *)d), "python.value");

  main_loop_sync_worker_startup_and_teardown();
  log_pipe_deinit((LogPipe *)d);
  log_pipe_unref((LogPipe *)d);
}

const gchar *python_fetcher_code = "\n\
from _syslogng import LogFetcher\n\
class Fetcher(LogFetcher):\n\
    @staticmethod\n\
    def generate_persist_name(options):\n\
        return options['key']\n\
    def fetch(self):\n\
        return LogFetcher.FETCH_NO_DATA, None";

Test(python_persist_name, test_python_fetcher)
{
  _load_code(python_fetcher_code);

  LogDriver *d = python_fetcher_new(empty_cfg);
  python_fetcher_set_class(d, "Fetcher");
  python_fetcher_set_option(d, "key", "value");
  cr_assert(log_pipe_init((LogPipe *)d));

  cr_assert_str_eq(log_pipe_get_persist_name((LogPipe *)d), "python-fetcher.value");

  main_loop_sync_worker_startup_and_teardown();
  log_pipe_deinit((LogPipe *)d);
  log_pipe_unref((LogPipe *)d);
}

const gchar *python_source_code = "\n\
from _syslogng import LogSource\n\
class Source(LogSource):\n\
    @staticmethod\n\
    def generate_persist_name(options):\n\
        return options['key']\n\
    def run(self):\n\
        pass\n\
    def request_exit(self):\n\
        pass";

Test(python_persist_name, test_python_source)
{
  _load_code(python_source_code);

  LogDriver *d = python_sd_new(empty_cfg);
  python_sd_set_class(d, "Source");
  python_sd_set_option(d, "key", "value");
  cr_assert(log_pipe_init((LogPipe *)d));

  cr_assert_str_eq(log_pipe_get_persist_name((LogPipe *)d), "python-source.value");

  main_loop_sync_worker_startup_and_teardown();
  log_pipe_deinit((LogPipe *)d);
  log_pipe_unref((LogPipe *)d);
}

const gchar *python_persist_generator_code = "\n\
def persist_generator_stats():\n\
    raise Exception('exception for testing stats')\n\
def persist_generator_persist():\n\
    raise Exception('exception for testing persist')";


Test(python_persist_name, test_python_exception_in_generate_persist_name)
{
  _load_code(python_persist_generator_code);

  LogPipe *p = (LogPipe *)python_sd_new(empty_cfg);
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  PyObject *persist_generator_stats = PyObject_GetAttrString(_py_get_main_module(python_config_get(empty_cfg)),
                                                             "persist_generator_stats");
  PyObject *persist_generator_persist = PyObject_GetAttrString(_py_get_main_module(python_config_get(empty_cfg)),
                                        "persist_generator_persist");
  PyGILState_Release(gstate);

  start_grabbing_messages();

  PythonPersistMembers options_stats =
  {
    .generate_persist_name_method = persist_generator_stats,
    .class = "class",
  };
  cr_assert_str_eq(python_format_stats_instance(p, "module", &options_stats), "module,class");

  PythonPersistMembers options_persist =
  {
    .generate_persist_name_method = persist_generator_persist,
    .class = "class",
  };
  cr_assert_str_eq(python_format_persist_name(p, "module", &options_persist), "module(class)");

  stop_grabbing_messages();

  assert_grabbed_log_contains("Exception: exception for testing stats");
  assert_grabbed_log_contains("Exception: exception for testing persist");

  Py_DECREF(persist_generator_stats);
  Py_DECREF(persist_generator_persist);
  log_pipe_unref((LogPipe *)p);
}

const gchar *python_fetcher_code_no_generate_persist_name = "\n\
from _syslogng import LogFetcher\n\
class Fetcher(LogFetcher):\n\
    def fetch(self):\n\
        return LogFetcher.FETCH_NO_DATA, None";

Test(python_persist_name, test_python_fetcher_no_generate_persist_name)
{
  _load_code(python_fetcher_code_no_generate_persist_name);

  LogDriver *d = python_fetcher_new(empty_cfg);
  python_fetcher_set_class(d, "Fetcher");
  python_fetcher_set_option(d, "key", "value");
  log_pipe_set_persist_name((LogPipe *)d, "test_persist_name");
  cr_assert(log_pipe_init((LogPipe *)d));

  cr_assert_str_eq(log_pipe_get_persist_name((LogPipe *)d), "python-fetcher.test_persist_name");

  main_loop_sync_worker_startup_and_teardown();
  log_pipe_deinit((LogPipe *)d);
  log_pipe_unref((LogPipe *)d);
}

const gchar *python_source_code_no_generate_persist_name = "\n\
from _syslogng import LogSource\n\
class Source(LogSource):\n\
    def run(self):\n\
        pass\n\
    def request_exit(self):\n\
        pass";

Test(python_persist_name, test_python_source_no_generate_persist_name)
{
  _load_code(python_source_code_no_generate_persist_name);

  LogDriver *d = python_sd_new(empty_cfg);
  python_sd_set_class(d, "Source");
  python_sd_set_option(d, "key", "value");
  log_pipe_set_persist_name((LogPipe *)d, "test_persist_name");
  cr_assert(log_pipe_init((LogPipe *)d));

  cr_assert_str_eq(log_pipe_get_persist_name((LogPipe *)d), "python-source.test_persist_name");

  main_loop_sync_worker_startup_and_teardown();
  log_pipe_deinit((LogPipe *)d);
  log_pipe_unref((LogPipe *)d);
}

const gchar *python_source_code_readonly = "\n\
from _syslogng import LogSource\n\
class Source(LogSource):\n\
    @staticmethod\n\
    def generate_persist_name(options):\n\
        return 'foo'\n\
    def init(self, options):\n\
        self.persist_name = 'readonly...'\n\
        return True\n\
    def run(self):\n\
        pass\n\
    def request_exit(self):\n\
        pass";

Test(python_persist_name, test_python_source_readonly)
{
  _load_code(python_source_code_readonly);

  LogDriver *d = python_sd_new(empty_cfg);
  python_sd_set_class(d, "Source");
  start_grabbing_messages();
  cr_assert_eq(log_pipe_init((LogPipe *)d), 0);
  stop_grabbing_messages();

  // Python2: TypeError: readonly attribute
  // Python3: AttributeError: readonly attribute
  assert_grabbed_log_contains("readonly attribute");

  main_loop_sync_worker_startup_and_teardown();
  log_pipe_deinit((LogPipe *)d);
  log_pipe_unref((LogPipe *)d);
}

const gchar *python_fetcher_code_readonly = "\n\
from _syslogng import LogFetcher\n\
class Fetcher(LogFetcher):\n\
    @staticmethod\n\
    def generate_persist_name(options):\n\
        return 'foo'\n\
    def init(self, options):\n\
        self.persist_name = 'readonly ...'\n\
        return True\n\
    def fetch(self):\n\
        return LogFetcher.FETCH_NO_DATA, None";

Test(python_persist_name, test_python_fetcher_readonly)
{
  _load_code(python_fetcher_code_readonly);

  LogDriver *d = python_fetcher_new(empty_cfg);
  python_fetcher_set_class(d, "Fetcher");

  start_grabbing_messages();
  cr_assert_eq(log_pipe_init((LogPipe *)d), 0);
  stop_grabbing_messages();

  // Python2: TypeError: readonly attribute
  // Python3: AttributeError: readonly attribute
  assert_grabbed_log_contains("readonly attribute");

  main_loop_sync_worker_startup_and_teardown();
  log_pipe_deinit((LogPipe *)d);
  log_pipe_unref((LogPipe *)d);
}

/* persist-name takes precedence over generate_persist_name */
Test(python_persist_name, test_python_fetcher_persist_preference)
{
  _load_code(python_fetcher_code);

  LogDriver *d = python_fetcher_new(empty_cfg);
  log_pipe_set_persist_name(&d->super, "test_persist_name");
  python_fetcher_set_class(d, "Fetcher");
  python_fetcher_set_option(d, "key", "value");
  cr_assert(log_pipe_init((LogPipe *)d));

  cr_assert_str_eq(log_pipe_get_persist_name((LogPipe *)d), "python-fetcher.test_persist_name");

  main_loop_sync_worker_startup_and_teardown();
  log_pipe_deinit((LogPipe *)d);
  log_pipe_unref((LogPipe *)d);
}

/* persist-name takes precedence over generate_persist_name */
Test(python_persist_name, test_python_source_persist_preference)
{
  _load_code(python_source_code);

  LogDriver *d = python_sd_new(empty_cfg);
  log_pipe_set_persist_name(&d->super, "test_persist_name");
  python_sd_set_class(d, "Source");
  python_sd_set_option(d, "key", "value");
  cr_assert(log_pipe_init((LogPipe *)d));

  cr_assert_str_eq(log_pipe_get_persist_name((LogPipe *)d), "python-source.test_persist_name");

  main_loop_sync_worker_startup_and_teardown();
  log_pipe_deinit((LogPipe *)d);
  log_pipe_unref((LogPipe *)d);
}
