/*
 * Copyright (c) 2023 Attila Szakacs
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

#include "python-reloc.h"
#include "python-types.h"
#include "reloc.h"

static PyObject *
py_get_installation_path_for(PyObject *self, PyObject *args, PyObject *kwargs)
{
  const gchar *template_str;
  static const gchar *kwlist[] = { "template", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", (gchar **) kwlist, &template_str))
    return NULL;

  const gchar *result_str = get_installation_path_for(template_str);
  if (!result_str)
    Py_RETURN_NONE;

  return py_string_from_string(result_str, -1);
}

static PyMethodDef py_reloc_methods[] =
{
  { "get_installation_path_for", (PyCFunction) py_get_installation_path_for, METH_VARARGS | METH_KEYWORDS, "Resolve an installation path template" },
  { NULL }
};

void
py_reloc_global_init(void)
{
  PyObject *module = PyImport_AddModule("_syslogng");
  PyModule_AddFunctions(module, py_reloc_methods);
}
