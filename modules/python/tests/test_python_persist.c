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
#include "persist_lib.h"

#include <criterion/criterion.h>

static PyObject *_python_main;
static PyObject *_python_main_dict;

static GlobalConfig *cfg;

CFG_LTYPE yyltype;
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

  py_init_threads();
  py_persist_init();
  PyEval_SaveThread();
}

static void
_load_code(const gchar *code)
{
  propagate_persist_state(cfg);

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  cr_assert(python_evaluate_global_code(cfg, code, &yyltype));
  PyGILState_Release(gstate);
}

void setup(void)
{
  app_startup();

  _py_init_interpreter();
  _init_python_main();

  cfg = cfg_new_snippet();
}

void teardown(void)
{
  cfg_free(cfg);
  app_shutdown();
}

TestSuite(python_persist, .init = setup, .fini = teardown);

const gchar *simple_persist = "\n\
from _syslogng import Persist\n\
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
  _load_code("from _syslogng import Persist");
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

Test(python_persist, test_python_persist_iterator)
{
  PersistState *state = clean_and_create_persist_state_for_test("test-python-iterator.persist");
  cfg->state = state;
  _load_code("from _syslogng import Persist");
  _load_code("persist = Persist('persist_name')");
  _load_code("persist['key1'] = 'value1'");
  _load_code("persist['key2'] = 'value2'");
  _load_code("persist['key3'] = 'value3'");
  _load_code("assert sorted(persist) == ['key1', 'key2', 'key3']");
  persist_state_stop(state);
}

Test(python_persist, test_python_persist_proper_types)
{
  PersistState *state = clean_and_create_persist_state_for_test("test-python-proper_types.persist");
  cfg->state = state;
  _load_code("from _syslogng import Persist");
  _load_code("persist = Persist('persist_name')");
  _load_code("persist['str'] = 'value'");
  _load_code("persist['number'] = 5");
  _load_code("assert persist['str'] == 'value'");
  _load_code("assert persist['number'] == 5");
  persist_state_stop(state);
}

const gchar *should_throw_exception = "\
from _syslogng import Persist\n\
persist = Persist('persist_name')\n\
exception_happened = False\n\
try:\n\
    persist['missing']\n\
except KeyError:\n\
    exception_happened = True\n\
assert exception_happened";

Test(python_persist, test_python_persist_lookup_missing_key)
{
  PersistState *state = clean_and_create_persist_state_for_test("test-python-lookup-missing-key.persist");
  cfg->state = state;
  _load_code(should_throw_exception);
  persist_state_stop(state);
};

const gchar *iter_returns_proper_types = "\n\
from _syslogng import Persist\n\
persist = Persist('persist_name')\n\
persist['integer'] = 1\n\
persist['str'] = 'str'\n\
persist['bytes'] = b'bytes'\n\
assert set([1, 'str', b'bytes']) == set([persist[k] for k in persist])";

Test(python_persist, test_python_persist_iter_returns_proper_types)
{
  PersistState *state = clean_and_create_persist_state_for_test("test-python-iter-returns-proper-types.persist");
  cfg->state = state;
  _load_code(iter_returns_proper_types);
  persist_state_stop(state);
};
