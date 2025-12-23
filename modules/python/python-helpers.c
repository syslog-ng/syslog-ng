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
#include "python-helpers.h"
#include "python-types.h"
#include "scratch-buffers.h"
#include "str-utils.h"
#include "messages.h"
#include "reloc.h"

const gchar *
_py_get_callable_name(PyObject *callable, gchar *buf, gsize buf_len)
{
  PyObject *exc, *value, *tb;

  PyErr_Fetch(&exc, &value, &tb);

  PyObject *name = PyObject_GetAttrString(callable, "__name__");

  const gchar *str;
  if (name && py_bytes_or_string_to_string(name, &str))
    {
      g_strlcpy(buf, str, buf_len);
    }
  else
    {
      PyErr_Clear();
      g_strlcpy(buf, "<unknown>", buf_len);
    }
  Py_XDECREF(name);

  PyErr_Restore(exc, value, tb);
  return buf;
}

void
_py_log_python_traceback_to_stderr(void)
{
  PyObject *traceback_module = NULL;
  PyObject *print_exception = NULL;
  PyObject *res = NULL;
  PyObject *exc, *value, *tb;

  PyErr_Fetch(&exc, &value, &tb);
  if (!exc)
    return;

  traceback_module = _py_do_import("traceback");
  if (!traceback_module)
    goto exit;
  print_exception = PyObject_GetAttrString(traceback_module, "print_exception");
  if (!print_exception)
    {
      msg_error("Error printing proper Python traceback for the exception, traceback.print_exception function not found");
      PyErr_Print();
      PyErr_Clear();
      goto exit;
    }
  res = PyObject_CallFunction(print_exception, "OOO", exc, value, tb ? : Py_None);
  if (!res)
    {
      msg_error("Error printing proper Python traceback for the exception, printing the error caused by print_exception() itself");
      PyErr_Print();
      PyErr_Clear();
    }
exit:
  Py_XDECREF(res);
  Py_XDECREF(print_exception);
  Py_XDECREF(traceback_module);
  PyErr_Restore(exc, value, tb);
}

const gchar *
_py_format_exception_text(gchar *buf, gsize buf_len)
{
  PyObject *exc, *value, *tb, *str;

  PyErr_Fetch(&exc, &value, &tb);
  if (!exc)
    {
      g_strlcpy(buf, "None", buf_len);
      return buf;
    }
  PyErr_NormalizeException(&exc, &value, &tb);

  str = PyObject_Str(value);
  if (!str)
    PyErr_Clear();

  const gchar *str_as_c_str;
  if (str && py_bytes_or_string_to_string(str, &str_as_c_str))
    {
      g_snprintf(buf, buf_len, "%s: %s", ((PyTypeObject *) exc)->tp_name, str_as_c_str);
    }
  else
    {
      g_strlcpy(buf, "<unknown>", buf_len);
    }
  Py_XDECREF(str);
  PyErr_Restore(exc, value, tb);
  return buf;
}

void
_py_finish_exception_handling(void)
{
  if (PyErr_ExceptionMatches(PyExc_ImportError))
    {
      PyObject *exc, *value, *tb;

      PyErr_Fetch(&exc, &value, &tb);
      PyImportErrorObject *import_error = (PyImportErrorObject *) value;
      const gchar *module_cstr;

      py_bytes_or_string_to_string(import_error->name, &module_cstr);
      msg_error("Seems you are missing a module that may be referenced by a "
                "syslog-ng plugin implemented in Python. These modules "
                "need to be installed either using your platform's package management "
                "tools (e.g. apt/dnf/yum) or Python's own package management "
                "tool (e.g. pip). syslog-ng authors recommend using pip and "
                "a dedicated Python virtualenv. You can initialize such a "
                "virtualenv using the `syslog-ng-update-virtualenv` command. "
                "This command will initialize the virtualenv and install all "
                "packages needed by plugins shipped with syslog-ng itself "
                "from the Python Package Index (PyPI). If you need any additional "
                "Python libraries for your local scripts, you can "
                "install those using the `pip` command located in the virtualenv's bin directory",
                evt_tag_str("module", module_cstr));
      PyErr_Restore(exc, value, tb);
    }

  _py_log_python_traceback_to_stderr();
  PyErr_Clear();
}

PyObject *
_py_get_attr_or_null(PyObject *o, const gchar *attr)
{
  PyObject *result;

  if (!attr)
    return NULL;

  result = PyObject_GetAttrString(o, attr);
  if (!result)
    {
      PyErr_Clear();
      return NULL;
    }
  return result;
}

PyObject *
_py_do_import(const gchar *modname)
{
  PyObject *module, *modobj;

  module = PyUnicode_FromString(modname);
  if (!module)
    {
      msg_error("Error allocating Python string",
                evt_tag_str("string", modname));
      return NULL;
    }

  modobj = PyImport_Import(module);
  Py_DECREF(module);
  if (modobj)
    {
      PyObject  *mod_filename = PyModule_GetFilenameObject(modobj);

      if (!mod_filename)
        {
          /* this exception only means that this module does not have a
           * __name__ attribute */
          PyErr_Clear();
        }

      msg_debug("python: importing Python module",
                evt_tag_str("module", modname),
                evt_tag_str("filename", mod_filename ? PyUnicode_AsUTF8(mod_filename) : "unknown"));
      Py_XDECREF(mod_filename);
    }
  else
    {
      gchar buf[256];

      msg_error("Error loading Python module",
                evt_tag_str("module", modname),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return NULL;
    }
  return modobj;
}

gboolean
_split_fully_qualified_name(const gchar *input, gchar **module, gchar **class)
{
  const gchar *p;

  for (p = input + strlen(input) - 1; p > input && *p != '.'; p--)
    ;

  if (p > input)
    {
      *module = g_strndup(input, (p - input));
      *class = g_strdup(p + 1);
      return TRUE;
    }
  return FALSE;
}

PyObject *
_py_resolve_qualified_name(const gchar *name)
{
  PyObject *module, *value = NULL;
  gchar *module_name, *attribute_name;

  if (!_split_fully_qualified_name(name, &module_name, &attribute_name))
    {
      module_name = g_strdup("_syslogng_main");
      attribute_name = g_strdup(name);
    }

  module = _py_do_import(module_name);
  if (!module)
    goto exit;

  value = _py_get_attr_or_null(module, attribute_name);
  Py_DECREF(module);

exit:
  g_free(module_name);
  g_free(attribute_name);
  return value;
}

static void
_insert_to_dict(gpointer key, gpointer value, gpointer dict)
{
  PyObject *key_pyobj = py_string_from_string((gchar *) key, -1);
  PyObject *value_pyobj = py_string_from_string((gchar *) value, -1);
  PyDict_SetItem( (PyObject *) dict, key_pyobj, value_pyobj);
  Py_XDECREF(key_pyobj);
  Py_XDECREF(value_pyobj);
}

PyObject *
_py_construct_cfg_args(CfgArgs *args)
{
  PyObject *arg_dict = PyDict_New();
  cfg_args_foreach(args, _insert_to_dict, arg_dict);
  return arg_dict;
}

PyObject *
_py_invoke_function(PyObject *func, PyObject *arg, const gchar *class, const gchar *caller_context)
{
  PyObject *ret;

  ret = PyObject_CallFunctionObjArgs(func, arg, NULL);
  if (!ret)
    {
      gchar buf1[256], buf2[256];

      msg_error("Exception while calling a Python function",
                evt_tag_str("caller", caller_context),
                evt_tag_str("class", class ? : "unknown"),
                evt_tag_str("function", _py_get_callable_name(func, buf1, sizeof(buf1))),
                evt_tag_str("exception", _py_format_exception_text(buf2, sizeof(buf2))));
      _py_finish_exception_handling();
      return NULL;
    }
  return ret;
}

PyObject *
_py_invoke_function_with_args(PyObject *func, PyObject *args, const gchar *class, const gchar *caller_context)
{
  PyObject *ret;

  ret = PyObject_CallObject(func, args);
  if (!ret)
    {
      gchar buf1[256], buf2[256];

      msg_error("Exception while calling a Python function",
                evt_tag_str("caller", caller_context),
                evt_tag_str("class", class),
                evt_tag_str("function", _py_get_callable_name(func, buf1, sizeof(buf1))),
                evt_tag_str("exception", _py_format_exception_text(buf2, sizeof(buf2))));
      _py_finish_exception_handling();
      return NULL;
    }
  return ret;
}

void
_py_invoke_void_function(PyObject *func, PyObject *arg, const gchar *class, const gchar *caller_context)
{
  PyObject *ret = _py_invoke_function(func, arg, class, caller_context);
  Py_XDECREF(ret);
}

gboolean
_py_invoke_bool_function(PyObject *func, PyObject *arg, const gchar *class, const gchar *caller_context)
{
  PyObject *ret;
  gboolean result = FALSE;

  ret = _py_invoke_function(func, arg, class, caller_context);
  if (ret)
    result = PyObject_IsTrue(ret);
  Py_XDECREF(ret);
  return result;
}

PyObject *
_py_get_optional_method(PyObject *instance, const gchar *class, const gchar *method_name, const gchar *module)
{
  PyObject *method = _py_get_attr_or_null(instance, method_name);
  if (!method)
    {
      msg_debug("Missing optional Python method",
                evt_tag_str("module", module),
                evt_tag_str("class", class),
                evt_tag_str("method", method_name));
      return NULL;
    }
  return method;
}

PyObject *
_py_invoke_method_by_name(PyObject *instance, const gchar *method_name, PyObject *arg, const gchar *class,
                          const gchar *module)
{
  PyObject *method = _py_get_optional_method(instance, class, method_name, module);

  if (!method)
    return NULL;

  PyObject *ret = _py_invoke_function(method, arg, class, module);
  Py_DECREF(method);

  return ret;
}

void
_py_invoke_void_method_by_name(PyObject *instance, const gchar *method_name, const gchar *class, const gchar *module)
{
  PyObject *method = _py_get_optional_method(instance, class, method_name, module);
  if (method)
    {
      _py_invoke_void_function(method, NULL, class, module);
      Py_DECREF(method);
    }
}

gboolean
_py_invoke_bool_method_by_name_with_options(PyObject *instance, const gchar *method_name,
                                            const PythonOptions *options, const gchar *class, const gchar *module)
{
  gboolean result = FALSE;
  PyObject *method = _py_get_optional_method(instance, class, method_name, module);

  if (method)
    {
      PyObject *py_options_dict = options ? python_options_create_py_dict(options) : NULL;
      result = _py_invoke_bool_function(method, py_options_dict, class, module);

      Py_XDECREF(py_options_dict);
      Py_DECREF(method);
    }
  return result;
}

gboolean
_py_invoke_bool_method_by_name(PyObject *instance, const gchar *method_name, const gchar *class, const gchar *module)
{
  return _py_invoke_bool_method_by_name_with_options(instance, method_name, NULL, class, module);
}

static void
_foreach_import(gpointer data, gpointer user_data)
{
  gchar *modname = (gchar *) data;
  PyObject *mod;

  mod = _py_do_import(modname);
  Py_XDECREF(mod);
}

gboolean
_py_perform_imports(GList *imports)
{
  g_list_foreach(imports, _foreach_import, NULL);
  return TRUE;
}

const gchar *
_py_object_repr(PyObject *s, gchar *buf, gsize buflen)
{
  const gchar *res;

  PyObject *r = PyObject_Repr(s);
  if (!r)
    {
      _py_finish_exception_handling();
      g_strlcpy(buf, "<unknown object>", buflen);
      return buf;
    }
  if (py_bytes_or_string_to_string(r, &res))
    g_strlcpy(buf, res, buflen);
  Py_XDECREF(r);
  return buf;
}

PyObject *
_py_construct_enum(const gchar *name, PyObject *sequence)
{
  PyObject *enum_module = PyImport_ImportModule("enum");

  if (!enum_module)
    return NULL;

  PyObject *enum_dict = PyModule_GetDict(enum_module);
  PyObject *enum_new = PyDict_GetItemString(enum_dict, "IntEnum");

  if (!enum_new)
    return NULL;

  PyObject *result = PyObject_CallFunction(enum_new, "sO", name, sequence);
  Py_XDECREF(enum_module);

  return result;
}


void
py_slng_generic_dealloc(PyObject *self)
{
  Py_TYPE(self)->tp_free(self);
}
