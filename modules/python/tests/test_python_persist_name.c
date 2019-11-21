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

#include "python-helpers.h"
#include "apphook.h"
#include "python-dest.h"
#include "python-main.h"
#include "python-fetcher.h"
#include "python-source.h"
#include "mainloop-worker.h"
#include "mainloop.h"
#include "python-persist.h"
#include "grab-logging.h"

#include <criterion/criterion.h>

MainLoop *main_loop;
MainLoopOptions main_loop_options = {0};

static PyObject *_python_main;
static PyObject *_python_main_dict;

YYLTYPE yyltype;
GlobalConfig *empty_cfg;

static void
_py_init_interpreter(void)
{
  Py_Initialize();
  py_init_argv();

  PyEval_InitThreads();
  py_log_fetcher_init();
  py_log_source_init();
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
  empty_cfg->filename  = g_strdup("dummy");
}

void teardown(void)
{
  cfg_free(empty_cfg);
  main_loop_deinit(main_loop);
  app_shutdown();
  Py_DECREF(_python_main);
  Py_DECREF(_python_main_dict);
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
class Dest(object):\n\
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
  PyObject *persist_generator_stats = PyObject_GetAttrString(_py_get_main_module(python_config_get(empty_cfg)),
                                                             "persist_generator_stats");
  PyObject *persist_generator_persist = PyObject_GetAttrString(_py_get_main_module(python_config_get(empty_cfg)),
                                        "persist_generator_persist");

  start_grabbing_messages();
  cr_assert_str_eq(python_format_stats_instance(p, persist_generator_stats,
                                                NULL, "module", "class"), "module,class");
  cr_assert_str_eq(python_format_persist_name(p, persist_generator_persist,
                                              NULL, "module", "class"), "module(class)");
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

const gchar *python_source_code_no_persist = "\n\
from _syslogng import LogSource\n\
class Source(LogSource):\n\
    def init(self, options):\n\
        self.persist_name\n\
        return True\n\
    def run(self):\n\
        pass\n\
    def request_exit(self):\n\
        pass";

Test(python_persist_name, test_python_source_no_persist)
{
  _load_code(python_source_code_no_persist);

  LogDriver *d = python_sd_new(empty_cfg);
  python_sd_set_class(d, "Source");
  start_grabbing_messages();
  cr_assert_eq(log_pipe_init((LogPipe *)d), 0);
  stop_grabbing_messages();
  display_grabbed_messages();

  assert_grabbed_log_contains("exceptions.AttributeError");

  main_loop_sync_worker_startup_and_teardown();
  log_pipe_deinit((LogPipe *)d);
  log_pipe_unref((LogPipe *)d);
}

const gchar *python_fetcher_code_no_persist = "\n\
from _syslogng import LogFetcher\n\
class Fetcher(LogFetcher):\n\
    def init(self, options):\n\
        self.persist_name\n\
        return True\n\
    def fetch(self):\n\
        return LogFetcher.FETCH_NO_DATA, None";

Test(python_persist_name, test_python_fetcher_no_persist)
{
  _load_code(python_fetcher_code_no_persist);

  LogDriver *d = python_fetcher_new(empty_cfg);
  python_fetcher_set_class(d, "Fetcher");

  start_grabbing_messages();
  cr_assert_eq(log_pipe_init((LogPipe *)d), 0);
  stop_grabbing_messages();
  display_grabbed_messages();

  assert_grabbed_log_contains("exceptions.AttributeError");

  main_loop_sync_worker_startup_and_teardown();
  log_pipe_deinit((LogPipe *)d);
  log_pipe_unref((LogPipe *)d);
}
