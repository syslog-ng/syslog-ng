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
#include "python-types.h"
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

/* String */

typedef struct _PythonOptionString
{
  PythonOption super;
  gchar *value;
} PythonOptionString;

static PyObject *
_string_create_value_py_object(const PythonOption *s)
{
  PythonOptionString *self = (PythonOptionString *) s;
  return py_string_from_string(self->value, -1);
}

static void
_string_free_fn(PythonOption *s)
{
  PythonOptionString *self = (PythonOptionString *) s;
  g_free(self->value);
}

PythonOption *
python_option_string_new(const gchar *name, const gchar *value)
{
  PythonOptionString *self = g_new0(PythonOptionString, 1);
  python_option_init_instance(&self->super, name);

  self->super.create_value_py_object = _string_create_value_py_object;
  self->super.free_fn = _string_free_fn;
  self->value = g_strdup(value);

  return &self->super;
}

/* Long */

typedef struct _PythonOptionLong
{
  PythonOption super;
  gint64 value;
} PythonOptionLong;

static PyObject *
_long_create_value_py_object(const PythonOption *s)
{
  PythonOptionLong *self = (PythonOptionLong *) s;
  return py_long_from_long(self->value);
}

PythonOption *
python_option_long_new(const gchar *name, gint64 value)
{
  PythonOptionLong *self = g_new0(PythonOptionLong, 1);
  python_option_init_instance(&self->super, name);

  self->super.create_value_py_object = _long_create_value_py_object;
  self->value = value;

  return &self->super;
}

/* Double */

typedef struct _PythonOptionDouble
{
  PythonOption super;
  gdouble value;
} PythonOptionDouble;

static PyObject *
_double_create_value_py_object(const PythonOption *s)
{
  PythonOptionDouble *self = (PythonOptionDouble *) s;
  return py_double_from_double(self->value);
}

PythonOption *
python_option_double_new(const gchar *name, gdouble value)
{
  PythonOptionDouble *self = g_new0(PythonOptionDouble, 1);
  python_option_init_instance(&self->super, name);

  self->super.create_value_py_object = _double_create_value_py_object;
  self->value = value;

  return &self->super;
}

/* Boolean */

typedef struct _PythonOptionBoolean
{
  PythonOption super;
  gboolean value;
} PythonOptionBoolean;

static PyObject *
_boolean_create_value_py_object(const PythonOption *s)
{
  PythonOptionBoolean *self = (PythonOptionBoolean *) s;
  return py_boolean_from_boolean(self->value);
}

PythonOption *
python_option_boolean_new(const gchar *name, gboolean value)
{
  PythonOptionBoolean *self = g_new0(PythonOptionBoolean, 1);
  python_option_init_instance(&self->super, name);

  self->super.create_value_py_object = _boolean_create_value_py_object;
  self->value = value;

  return &self->super;
}
