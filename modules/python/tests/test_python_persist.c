/*
 * Copyright (c) 2020 Balabit
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

#include "python-persist.h"
#include "python-main.h"
#include "apphook.h"
#include "mainloop.h"
#include "persist_lib.h"

#include <criterion/criterion.h>

MainLoop *main_loop;
MainLoopOptions main_loop_options = {0};

static PyObject *_python_main;
static PyObject *_python_main_dict;

static GlobalConfig *cfg;

YYLTYPE yyltype;
GlobalConfig *empty_cfg;

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

static void
_py_init_interpreter(void)
{
  Py_Initialize();
  py_init_argv();

  PyEval_InitThreads();
  py_persist_init();
  PyEval_SaveThread();
}

static void
_load_code(const gchar *code)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  cr_assert(python_evaluate_global_code(cfg, code, &yyltype));
  PyGILState_Release(gstate);
}

void setup(void)
{
  app_startup();

  main_loop = main_loop_get_instance();
  main_loop_init(main_loop, &main_loop_options);

  cfg = main_loop_get_current_config(main_loop_get_instance());

  _py_init_interpreter();
  _init_python_main();

  _load_code("from _syslogng import Persist");
}

void teardown(void)
{
  main_loop_deinit(main_loop);
  app_shutdown();
}

TestSuite(python_persist, .init = setup, .fini = teardown);

const gchar *simple_persist = "\n\
class SubPersist(Persist):\n\
    def __init__(self, persist_name):\n\
        super(SubPersist, self).__init__(persist_name = persist_name)\n\
\n\
persist = SubPersist('foo')\n\
assert persist.persist_name == 'foo', 'wrong persist name'";

void
persist_state_stop(PersistState *state)
{
  gchar *filename = g_strdup(persist_state_get_filename(state));

  persist_state_cancel(state);

  unlink(filename);
  g_free(filename);
};

Test(python_persist, test_python_persist_name)
{
  PersistState *state = clean_and_create_persist_state_for_test("test-python-persist-name.persist");
  cfg->state = state;
  _load_code(simple_persist);
  persist_state_stop(state);
}

Test(python_persist, test_python_persist_basic)
{
  PersistState *state = clean_and_create_persist_state_for_test("test-python.persist");
  cfg->state = state;
  _load_code("persist = Persist('persist_name')");
  _load_code("assert 'key' not in persist");
  _load_code("persist['key'] = 'value'");
  _load_code("assert 'key' in persist");
  cfg->state = restart_persist_state(state);
  _load_code("persist = Persist('persist_name')");
  _load_code("persist['key'] = 'value'");
  _load_code("assert 'key' in persist");
  _load_code("assert persist['key'] == 'value'");
  persist_state_stop(cfg->state);
}
