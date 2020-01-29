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
 *
 */

#include "python-persist.h"
#include "python-helpers.h"
#include "driver.h"

static PyObject *
_call_generate_persist_name_method(PythonPersistMembers *options)
{
  PyObject *py_options = options->options ? _py_create_arg_dict(options->options) : NULL;
  PyObject *ret = _py_invoke_function(options->generate_persist_name_method, py_options,
                                      options->class, options->id);
  Py_XDECREF(py_options);
  return ret;
}

static void
format_default_stats_instance(gchar *buffer, gsize size, const gchar *module, const gchar *name)
{
  g_snprintf(buffer, size, "%s,%s", module, name);
}

static void
copy_stats_instance(const LogPipe *self, const gchar *module, PythonPersistMembers *options,
                    gchar *buffer, gsize size)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyObject *ret = _call_generate_persist_name_method(options);
  if (ret)
    g_snprintf(buffer, size, "%s,%s", module, _py_get_string_as_string(ret));
  else
    {
      format_default_stats_instance(buffer, size, module, options->class);
      msg_error("Failed while generating persist name, using default",
                evt_tag_str("default_persist_name", buffer),
                evt_tag_str("driver", options->id),
                evt_tag_str("class", options->class));
    }
  Py_XDECREF(ret);

  PyGILState_Release(gstate);
}

const gchar *
python_format_stats_instance(LogPipe *p, const gchar *module, PythonPersistMembers *options)
{
  static gchar persist_name[1024];

  if (p->persist_name)
    format_default_stats_instance(persist_name, sizeof(persist_name), module, p->persist_name);
  else if (options->generate_persist_name_method)
    copy_stats_instance(p, module, options, persist_name, sizeof(persist_name));
  else
    format_default_stats_instance(persist_name, sizeof(persist_name), module, options->class);

  return persist_name;
}

static void
format_default_persist_name_with_persist_name(gchar *buffer, gsize size, const gchar *module, const gchar *persist_name)
{
  g_snprintf(buffer, size, "%s.%s", module, persist_name);
}

static void
format_default_persist_name_with_class(gchar *buffer, gsize size, const gchar *module, const gchar *class)
{
  g_snprintf(buffer, size, "%s(%s)", module, class);
}

static void
copy_persist_name(const LogPipe *self, const gchar *module, PythonPersistMembers *options,
                  gchar *buffer, gsize size)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyObject *ret =_call_generate_persist_name_method(options);
  if (ret)
    g_snprintf(buffer, size, "%s.%s", module, _py_get_string_as_string(ret));
  else
    {
      format_default_persist_name_with_class(buffer, size, module, options->class);
      msg_error("Failed while generating persist name, using default",
                evt_tag_str("default_persist_name", buffer),
                evt_tag_str("driver", options->id),
                evt_tag_str("class", options->class));
    }
  Py_XDECREF(ret);

  PyGILState_Release(gstate);
}

const gchar *
python_format_persist_name(const LogPipe *p, const gchar *module, PythonPersistMembers *options)
{
  static gchar persist_name[1024];

  if (p->persist_name)
    format_default_persist_name_with_persist_name(persist_name, sizeof(persist_name), module, p->persist_name);
  else if (options->generate_persist_name_method)
    copy_persist_name(p, module, options, persist_name, sizeof(persist_name));
  else
    format_default_persist_name_with_class(persist_name, sizeof(persist_name), module, options->class);

  return persist_name;
}
