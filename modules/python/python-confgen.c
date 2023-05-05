/*
 * Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
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
#include "python-module.h"
#include "python-helpers.h"
#include "python-types.h"
#include "cfg-block-generator.h"
#include "cfg-lexer.h"
#include "cfg.h"

typedef struct _PythonConfigGenerator
{
  CfgBlockGenerator super;
  PyObject *py_generator_function;
} PythonConfigGenerator;

static gboolean
python_config_generator_generate(CfgBlockGenerator *s,
                                 GlobalConfig *cfg,
                                 gpointer a,
                                 GString *result,
                                 const gchar *reference)
{
  PythonConfigGenerator *self = (PythonConfigGenerator *) s;
  PyGILState_STATE gstate;
  CfgArgs *args = (CfgArgs *) a;
  gboolean success = FALSE;

  gstate = PyGILState_Ensure();
  PyObject *py_args = _py_construct_cfg_args((CfgArgs *) args);
  PyObject *res = _py_invoke_function(self->py_generator_function, py_args, NULL, reference);
  Py_XDECREF(py_args);

  if (res)
    {
      const gchar *generated_config;
      if (py_bytes_or_string_to_string(res, &generated_config))
        {
          g_string_assign(result, generated_config);
          success = TRUE;
        }
      else
        {
          msg_error("python-confgen: generator function returned a non-string value",
                    evt_tag_str("reference", reference));
        }
      Py_DECREF(res);
    }
  PyGILState_Release(gstate);
  return success;
}

static void
python_config_generator_free(CfgBlockGenerator *s)
{
  PythonConfigGenerator *self = (PythonConfigGenerator *) s;
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  Py_XDECREF(self->py_generator_function);
  PyGILState_Release(gstate);
  cfg_block_generator_free_instance(s);
}

CfgBlockGenerator *
python_config_generator_new(gint context, const gchar *name, PyObject *py_generator_function)
{
  PythonConfigGenerator *self = g_new0(PythonConfigGenerator, 1);

  cfg_block_generator_init_instance(&self->super, context, name);
  self->super.generate = python_config_generator_generate;
  self->super.free_fn = python_config_generator_free;
  self->py_generator_function = py_generator_function;
  return &self->super;
}

void
python_confgen_register(gint context, const gchar *name, PyObject *py_generator_function)
{
  CfgBlockGenerator *confgen;

  confgen = python_config_generator_new(context, name, py_generator_function);
  cfg_lexer_register_generator_plugin(&configuration->plugin_context, confgen);
}

static PyObject *
py_register_config_generator(PyObject *self, PyObject *args, PyObject *kwargs)
{
  PyObject *generator_function;
  gchar *context_name;
  gchar *name;

  static const gchar *kwlist[] = { "context", "name", "config_generator", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ssO", (gchar **) kwlist,
                                   &context_name, &name, &generator_function))
    return NULL;

  gint context_value = cfg_lexer_lookup_context_type_by_name(context_name);
  if (!context_value)
    {
      PyErr_Format(PyExc_ValueError, "unknown context value %s", context_name);
      return NULL;
    }

  Py_XINCREF(generator_function);
  python_confgen_register(context_value, name, generator_function);

  Py_RETURN_NONE;
}

static PyMethodDef py_confgen_methods[] =
{
  { "register_config_generator", (PyCFunction) py_register_config_generator, METH_VARARGS | METH_KEYWORDS, "Register a config generator plugin" },
  { NULL }
};

void
py_init_confgen(void)
{
  PyObject *module = PyImport_AddModule("_syslogng");
  PyModule_AddFunctions(module, py_confgen_methods);
}
