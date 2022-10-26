/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
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
#include "python-main.h"
#include "python-module.h"
#include "python-helpers.h"
#include "python-global-code-loader.h"
#include "python-types.h"

#include "python-dest.h"
#include "python-source.h"
#include "python-fetcher.h"
#include "python-logparser.h"
#include "python-tf.h"
#include "python-logmsg.h"
#include "python-logtemplate.h"
#include "python-integerpointer.h"
#include "python-logger.h"
#include "python-persist.h"
#include "python-bookmark.h"
#include "python-ack-tracker.h"
#include "python-debugger.h"

#include "cfg.h"
#include "messages.h"
#include "reloc.h"

/*
 * Some information about how we embed the Python interpreter:
 *  - instead of using the __main__ module, we use a separate _syslogng_main
 *    module as we want to replace it every time syslog-ng is reloaded
 *  - handling of our separate main module is implemented by this module
 *  - this separate __main__ module requires some magic though (most of it
 *    is in _py_construct_main_module(), see below.
 *  - the PyObject reference to the current main module is stored in the
 *    GlobalConfig instance (using the ModuleConfig mechanism)
 *  - the current main module is switched during config initialization, e.g.
 *    during runtime, _syslogng_main module refers to the main module of the current
 *    config
 *  - since _syslogng_main is different for each initialized syslog-ng
 *    configuration, it is not possible to use variables between reloads,
 *    _except_ by storing them in other, imported modules.
 *  - it is currently not possible to hook into the init/deinit mechanism in
 *    the global context (from Python), but it is possible for a DestDriver.
 *    It is doable also in the global context, but has not yet been done.
 */

static PyObject *
_py_construct_main_module(PythonConfig *pc)
{
  PyObject *module;
  PyObject *module_dict;
  PyObject *modules = PyImport_GetModuleDict();

  /* make sure the module registry doesn't contain our module */
  if (PyDict_DelItemString(modules, "_syslogng_main") < 0)
    PyErr_Clear();

  /* create a new module */
  module = PyImport_AddModule("_syslogng_main");
  if (!module)
    {
      gchar buf[256];

      msg_error("Error creating syslog-ng main module",
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return NULL;
    }

  /* make sure __builtins__ is there, it is normally automatically done for
   * __main__ and any imported modules */
  module_dict = PyModule_GetDict(module);
  if (PyDict_GetItemString(module_dict, "__builtins__") == NULL)
    {
      PyObject *builtins_module = PyImport_ImportModule(PYTHON_BUILTIN_MODULE_NAME);
      if (builtins_module == NULL ||
          PyDict_SetItemString(module_dict, "__builtins__", builtins_module) < 0)
        g_assert_not_reached();
      Py_XDECREF(builtins_module);
    }

  /* there's a circular reference between GlobalConfig -> PythonConfig -> main_module -> main_module.__config__ -> GlobalConfig */
  PyDict_SetItemString(module_dict, "__config__", PyCapsule_New(pc->cfg, "_syslogng_main.__config__", NULL));

  /* return a reference */
  Py_INCREF(module);
  return module;
}

/* switch the _syslogng_main module to the one in a specific configuration */
void
_py_switch_main_module(PythonConfig *pc)
{
  PyObject *modules = PyImport_GetModuleDict();

  if (pc->main_module)
    {
      Py_INCREF(pc->main_module);
      PyDict_SetItemString(modules, "_syslogng_main", pc->main_module);
    }
  else
    {
      PyDict_DelItemString(modules, "_syslogng_main");
    }
}


/* get the current main module in a production context by using the Python
 * module registry instead of the GlobalConfig machinery.  This only works
 * once _py_switch_to_main_module() has been called (e.g. past init()).
 *
 * returns borrowed reference. */
PyObject *
_py_get_current_main_module(void)
{
  return PyImport_AddModule("_syslogng_main");
}

/* get the current main module as stored in the current GlobalConfig.
 * returns borrowed reference */
PyObject *
_py_get_main_module(PythonConfig *pc)
{
  if (!pc->main_module)
    pc->main_module = _py_construct_main_module(pc);
  return pc->main_module;
}

gboolean
_py_evaluate_global_code(PythonConfig *pc, const gchar *filename, const gchar *code)
{
  PyObject *module;

  module = _py_get_main_module(pc);
  if (!module)
    return FALSE;

  /* NOTE: this has become a tad more difficult than PyRun_SimpleString(),
   * because we want proper backtraces originating from this module.  Should
   * we only use PyRun_SimpleString(), we would not get source lines in our
   * backtraces.
   *
   * This implementation basically mimics what an import would do, compiles
   * the code as it was coming from a "virtual" file and then executes the
   * compiled code in the context of the _syslogng_main module.
   *
   * Since we attach a __loader__ to the module, that will be called when
   * the source is needed to report backtraces.  Its implementation is in
   * the python-global-code-loader.c, that simply returns the source when
   * its get_source() method is called.
   */
  PyObject *dict = PyModule_GetDict(module);
  PyDict_SetItemString(dict, "__loader__", py_global_code_loader_new(code));

  PyObject *code_object = Py_CompileString((char *) code, filename, Py_file_input);
  if (!code_object)
    {
      gchar buf[256];

      msg_error("Error compiling Python global code block",
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }
  module = PyImport_ExecCodeModuleEx("_syslogng_main", code_object, (char *) filename);
  Py_DECREF(code_object);

  if (!module)
    {
      gchar buf[256];

      msg_error("Error evaluating global Python block",
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return FALSE;
    }
  return TRUE;
}

GlobalConfig *
python_get_associated_config(void)
{
  GlobalConfig *cfg;

  cfg = (GlobalConfig *) PyCapsule_Import("_syslogng_main.__config__", FALSE);
  g_assert(cfg != NULL);
  return cfg;
}

gboolean
python_evaluate_global_code(GlobalConfig *cfg, const gchar *code, CFG_LTYPE *yylloc)
{
  PyGILState_STATE gstate;
  gchar buf[256];
  PythonConfig *pc = python_config_get(cfg);
  gboolean result;

  gstate = PyGILState_Ensure();
  g_snprintf(buf, sizeof(buf), "%s{python-global-code:%d}", cfg->filename, yylloc->first_line);
  result = _py_evaluate_global_code(pc, buf, code);
  PyGILState_Release(gstate);

  return result;
}

static gboolean interpreter_initialized = FALSE;

static gchar *
_format_python_path(void)
{
  const gchar *current_python_path = getenv("PYTHONPATH");
  GString *python_path = g_string_new("");

  g_string_printf(python_path, "%s:%s",
                  get_installation_path_for(SYSLOG_NG_PYTHON_SYSCONF_MODULE_DIR),
                  get_installation_path_for(SYSLOG_NG_PYTHON_MODULE_DIR));

  if (current_python_path)
    g_string_append_printf(python_path, ":%s", current_python_path);
  return g_string_free(python_path, FALSE);
}

static gboolean
_py_set_python_path(PyConfig *config)
{
  gchar *python_path = _format_python_path();
  PyStatus status = PyConfig_SetBytesString(config, &config->pythonpath_env, python_path);
  g_free(python_path);

  if (PyStatus_Exception(status))
    {
      msg_error("Error initializing Python, setting PYTHONPATH failed",
                evt_tag_str("func", status.func),
                evt_tag_str("error", status.err_msg),
                evt_tag_int("exitcode", status.exitcode));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_py_set_python_home(PyConfig *config)
{
#ifdef SYSLOG_NG_PYTHON3_HOME_DIR
  if (strlen(SYSLOG_NG_PYTHON3_HOME_DIR) > 0)
    {
      const gchar *resolved_python_home = get_installation_path_for(SYSLOG_NG_PYTHON3_HOME_DIR);
      PyStatus status = PyConfig_SetBytesString(config, &config->home, resolved_python_home);
      if (PyStatus_Exception(status))
        {
          msg_error("Error initializing Python, setting PYTHONHOME failed",
                    evt_tag_str("func", status.func),
                    evt_tag_str("error", status.err_msg),
                    evt_tag_int("exitcode", status.exitcode));
          return FALSE;
        }
    }
#endif
  return TRUE;
}

static gboolean
_py_is_virtualenv_valid(const gchar *path)
{
  gchar *python_venv_binary = g_strdup_printf("%s/bin/python", path);
  gboolean result = FALSE;

  /* check if the virtualenv is populated */
  if (g_file_test(path, G_FILE_TEST_IS_DIR) &&
      g_file_test(python_venv_binary, G_FILE_TEST_IS_EXECUTABLE))
    {
      result = TRUE;
    }

  g_free(python_venv_binary);
  return result;
}

static gboolean
_py_set_argv(PyConfig *config, const gchar *binary)
{
  char *argv[] = { (char *) binary };
  PyStatus status = PyConfig_SetBytesArgv(config, 1, argv);

  if (PyStatus_Exception(status))
    {
      msg_error("Error initializing Python, PyConfig_SetBytesArgv() failed",
                evt_tag_str("func", status.func),
                evt_tag_str("error", status.err_msg),
                evt_tag_int("exitcode", status.exitcode));
      return FALSE;
    }
  return TRUE;
}

static gboolean
_py_activate_venv(PyConfig *config)
{
  const gchar *env_virtual_env = getenv("VIRTUAL_ENV");

  /* check if VIRTUAL_ENV points to a valid virtualenv */
  if (env_virtual_env &&
      !_py_is_virtualenv_valid(env_virtual_env))
    {
      msg_error("python: environment variable VIRTUAL_ENV is set, but does not point to a valid virtualenv, Python executable not found",
                evt_tag_str("path", env_virtual_env));
      return FALSE;
    }

  const gchar *python_venv_path = get_installation_path_for(SYSLOG_NG_PYTHON_VENV_DIR);
  if (!_py_is_virtualenv_valid(python_venv_path))
    {
      msg_debug("python: private virtualenv is not initialized, relying on system-installed Python packages",
                evt_tag_str("path", python_venv_path));
      return TRUE;
    }

  gchar *python_venv_binary = g_strdup_printf("%s/bin/python", python_venv_path);

  msg_info("python: activating virtualenv",
           evt_tag_str("path", python_venv_path),
           evt_tag_str("executable", python_venv_binary));

  /* Python will detect the virtual env based on its sys.executable value,
   * which we are setting through PyConfig_SetBytesArgv().  The algorithm is
   * described in site.py (and various PEPs and documentation).
   *
   * We basically behave as if the user executed
   * ${python_venvdir}/bin/python when it started syslog-ng.
   */

  gboolean success = _py_set_argv(config, python_venv_binary);
  g_free(python_venv_binary);
  return success;
}

/*
 * NOTE: we can have the following embedded Python deployment:
 *
 *   1) private Python installation: we have a complete Python installation
 *      (from source) usually compiled in the same ${prefix} as we are.  In
 *      this case, PYTHONHOME needs to point to the Python prefix.
 *      syslog-ng supports this case via the PYTHON3_HOME_DIR configure
 *      option which you need to set in this case.
 *
 *      In this case we are not using any Python binaries (libpython.so) nor
 *      Python packages from the system. Everything is private.
 *
 *       - Python binaries are private, using a custom prefix
 *       - Python packages are in ${PYTHONHOME}/lib/pythonX.Y
 *       - syslog-ng Python code is in ${moduledir}/python
 *
 *   2) system Python + system installed Python packages: we are relying on
 *      the system Python installation both for binaries and packages.  This
 *      roughly match the distribution model: all Python packages are
 *      installed via system packages (to /usr/lib/pythonX.Y) OR by using pip
 *      to augment the system installation (/usr/local/lib/pythonX.Y)
 *
 *       - Python binaries are the system ones, usually /usr/bin/python and
 *         /usr/lib/libpython.so (or their versioned equivalents)
 *       - Python packages are in /usr/lib/pythonX.Y/
 *         and their /usr/local counterparts (e.g.  the standard paths for
 *         system Python)
 *       - syslog-ng Python code is in ${moduledir}/python
 *       - all imports performed by syslog-ng Python modules will be
 *         satisfied through system paths, which you can populate either by
 *         install python packages or via the installation of pip packages
 *         into /usr/local
 *
 *   3) system Python + virtualenv: in this mode, we have our own private
 *      virtualenv where you can deploy packages privately, without making
 *      them visible globally on the system.
 *
 *       - Python binaries are the system ones, usually /usr/bin/python and
 *         /usr/lib/libpython.so (or their versioned equivalents)
 *       - Python packages are in our virtualenv folder, system Python
 *         packages are either accessible or isolated (depending on the
 *         virtualenv's --system-site-packages option)
 *       - syslog-ng Python code is in ${moduledir}/python
 *
 * PYTHONPATH is popuolated as follows
 *   1. /etc/syslog-ng/python      (${sysconfdir}/python)
 *   2. /usr/lib/syslog-ng/python  (${exec_prefix}/lib/syslog-ng/python)
 *   3. $PYTHONPATH as passed by the user in the environment
 *   4. automatic Python path set by the interpreter (venv or system)
 */
static gboolean
_py_init_embedded_python_environment(void)
{
  PyConfig config;
  PyConfig_InitIsolatedConfig(&config);
  if (!_py_set_python_path(&config) ||
      !_py_set_python_home(&config) ||
      !_py_activate_venv(&config))
    return FALSE;

  Py_InitializeFromConfig(&config);
  PyConfig_Clear(&config);
  return TRUE;
}

static void
_py_initialize_builtin_modules(void)
{
  py_init_threads();
  py_init_types();

  py_log_message_global_init();
  py_log_template_global_init();
  py_integer_pointer_global_init();
  py_log_destination_global_init();
  py_log_parser_global_init();
  py_log_source_global_init();
  py_log_fetcher_global_init();
  py_persist_global_init();
  py_bookmark_global_init();
  py_ack_tracker_global_init();
  py_global_code_loader_global_init();
  py_logger_global_init();
}

gboolean
_py_init_interpreter(void)
{
  if (!interpreter_initialized)
    {
      python_debugger_append_inittab();
      if (!_py_init_embedded_python_environment())
        return FALSE;

      _py_initialize_builtin_modules();
      PyEval_SaveThread();

      interpreter_initialized = TRUE;
    }
  return TRUE;
}
