/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#include "python-startup.h"
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
#include "python-confgen.h"
#include "python-global-code-loader.h"
#include "python-types.h"
#include "python-reloc.h"

#include "reloc.h"

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
_py_requirements_up_to_date(const gchar *python_venv_path)
{
  const gchar *my_requirements_file = get_installation_path_for(SYSLOG_NG_PYTHON_MODULE_DIR "/requirements.txt");
  gchar *installed_requirements_file = g_strdup_printf("%s/requirements.txt", python_venv_path);
  gchar *installed_requirements = NULL;
  gsize installed_requirements_len;
  gchar *my_requirements = NULL;
  gsize my_requirements_len;
  gboolean result = FALSE;

  if (!g_file_get_contents(installed_requirements_file, &installed_requirements, &installed_requirements_len, NULL))
    goto mismatch;

  if (!g_file_get_contents(my_requirements_file, &my_requirements, &my_requirements_len, NULL))
    goto mismatch;

  if (my_requirements_len != installed_requirements_len ||
      strcmp(my_requirements, installed_requirements) != 0)
    goto mismatch;

  result = TRUE;
mismatch:
  g_free(installed_requirements);
  g_free(my_requirements);
  g_free(installed_requirements_file);
  return result;
}

static const gchar *
_get_venv_path(void)
{
  const gchar *env_virtual_env = getenv("VIRTUAL_ENV");
  if (env_virtual_env)
    {
      if (!_py_is_virtualenv_valid(env_virtual_env))
        {
          msg_error("python: environment variable VIRTUAL_ENV is set, but does not point to a valid virtualenv, Python executable not found",
                    evt_tag_str("path", env_virtual_env));
          return NULL;
        }

      msg_debug("python: using virtualenv pointed to by $VIRTUAL_ENV", evt_tag_str("path", env_virtual_env));
      return env_virtual_env;
    }

  const gchar *private_venv_path = get_installation_path_for(SYSLOG_NG_PYTHON_VENV_DIR);
  if (!_py_is_virtualenv_valid(private_venv_path))
    {
      msg_debug("python: private virtualenv is not initialized, use the `syslog-ng-update-virtualenv' "
                "script to initialize it or make sure all required Python dependencies are available in "
                "the system Python installation",
                evt_tag_str("path", private_venv_path));
      return NULL;
    }

  if (!_py_requirements_up_to_date(private_venv_path))
    {
      msg_warning("python: the current set of requirements installed in our virtualenv seems to be out of date, "
                  "use the `syslog-ng-update-virtualenv' script to upgrade Python dependencies",
                  evt_tag_str("path", private_venv_path));
      return NULL;
    }

  msg_debug("python: the virtualenv validation successful");
  return private_venv_path;
}

#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 8 || PY_MAJOR_VERSION > 3

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
  const gchar *python_venv_path = _get_venv_path();

  gboolean success;
  if (python_venv_path)
    {
      /* Python will detect the virtual env based on its sys.executable value,
       * which we are setting through PyConfig_SetBytesArgv().  The algorithm is
       * described in site.py (and various PEPs and documentation).
       *
       * We basically behave as if the user executed
       * ${python_venvdir}/bin/python when it started syslog-ng.
       */
      gchar *python_venv_binary = g_strdup_printf("%s/bin/python", python_venv_path);

      msg_debug("python: activating virtualenv",
                evt_tag_str("path", python_venv_path),
                evt_tag_str("executable", python_venv_binary));
      success = _py_set_argv(config, python_venv_binary);
      g_free(python_venv_binary);
    }
  else
    {
      success = _py_set_argv(config, "syslog-ng");
    }

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
_py_configure_virtualenv_python(PyConfig *config)
{
  if (!_py_set_python_path(config) ||
      !_py_set_python_home(config) ||
      !_py_activate_venv(config))
    return FALSE;
  return TRUE;
}

static gboolean
_py_configure_system_python(PyConfig *config)
{
  if (!_py_set_python_path(config) ||
      !_py_set_python_home(config) ||
      !_py_set_argv(config, "syslog-ng"))
    return FALSE;
  return TRUE;
}

static gboolean
_py_configure_interpreter(gboolean use_virtualenv)
{
  PyConfig config;
  PyConfig_InitPythonConfig(&config);

  /* NOTE: to pick up PYTHONPATH, work around https://github.com/python/cpython/issues/101471 */
  config.configure_c_stdio = 0;
  config.install_signal_handlers = 0;
  config.parse_argv = 0;
  config.pathconfig_warnings = 0;
  config.user_site_directory = 0;
  config.use_environment = 1;
  gboolean success = use_virtualenv ? _py_configure_virtualenv_python(&config)
                     : _py_configure_system_python(&config);
  if (success)
    {
      Py_InitializeFromConfig(&config);
      PyConfig_Clear(&config);
      return TRUE;
    }
  return FALSE;
}

#else

/* Python older than v3.8 lacks the PyConfig based API */

static void
_py_setup_python_home(void)
{
#ifdef SYSLOG_NG_PYTHON3_HOME_DIR
  if (strlen(SYSLOG_NG_PYTHON3_HOME_DIR) > 0)
    {
      const gchar *resolved_python_home = get_installation_path_for(SYSLOG_NG_PYTHON3_HOME_DIR);
      Py_SetPythonHome(Py_DecodeLocale(resolved_python_home, NULL));
    }
#endif
}

static void
_py_set_python_path(void)
{
  gchar *python_path = _format_python_path();
  setenv("PYTHONPATH", python_path, 1);
  g_free(python_path);
}

static void
_py_init_argv(void)
{
  static wchar_t *argv[] = {L"syslog-ng"};
  PySys_SetArgvEx(1, argv, 0);
}

static gboolean
_py_activate_venv(void)
{
  const gchar *python_venv_path = _get_venv_path();

  if (!python_venv_path)
    return FALSE;

  gchar *python_venv_binary = g_build_path(G_DIR_SEPARATOR_S, python_venv_path, "bin", "python", NULL);
  wchar_t *python_programname = Py_DecodeLocale(python_venv_binary, NULL);

  msg_debug("python: activating virtualenv",
            evt_tag_str("path", python_venv_path),
            evt_tag_str("executable", python_venv_binary));
  g_free(python_venv_binary);

  Py_SetProgramName(python_programname);
  return TRUE;
}

static gboolean
_py_configure_interpreter(gboolean use_virtualenv)
{
  _py_setup_python_home();
  _py_set_python_path();

  if (use_virtualenv)
    _py_activate_venv();

  Py_Initialize();
  _py_init_argv();
  return TRUE;
}

#endif

static void
_py_initialize_builtin_modules(void)
{
  py_init_threads();
  py_init_types();
  py_init_confgen();

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
  py_reloc_global_init();
  py_global_code_loader_global_init();
  py_logger_global_init();
}

gboolean
_py_init_interpreter(gboolean use_virtualenv)
{
  if (!interpreter_initialized)
    {
      python_debugger_append_inittab();

      if (!_py_configure_interpreter(use_virtualenv))
        return FALSE;

      _py_initialize_builtin_modules();

      PyEval_SaveThread();

      interpreter_initialized = TRUE;
    }
  return TRUE;
}
