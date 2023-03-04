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
 *
 */

#include "python-options.h"
#include "str-utils.h"

/* Python Option */

struct _PythonOption
{
  gchar *name;

  PyObject *(*create_value_py_object)(const PythonOption *s);
  void (*free_fn)(PythonOption *s);
};

static void
python_option_init_instance(PythonOption *s, const gchar *name)
{
  s->name = __normalize_key(name);
}

const gchar *
python_option_get_name(const PythonOption *s)
{
  return s->name;
}

PyObject *
python_option_create_value_py_object(const PythonOption *s)
{
  g_assert(s->create_value_py_object);

  PyObject *value;

  PyGILState_STATE gstate = PyGILState_Ensure();
  {
    value = s->create_value_py_object(s);
    if (!value)
      {
        gchar buf[256];
        msg_error("python-options: error converting option to Python object",
                  evt_tag_str("option", python_option_get_name(s)),
                  evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
        _py_finish_exception_handling();
      }
  }
  PyGILState_Release(gstate);

  return value;
}

void
python_option_free(PythonOption *s)
{
  if (s->free_fn)
    s->free_fn(s);

  g_free(s->name);
  g_free(s);
}
