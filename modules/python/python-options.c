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
#include "python-helpers.h"
#include "python-logtemplate.h"
#include "str-utils.h"
#include "string-list.h"
#include "atomic.h"


/* Python Option */

struct _PythonOption
{
  GAtomicCounter ref_cnt;
  gchar *name;

  PyObject *(*create_value_py_object)(const PythonOption *s);
  void (*free_fn)(PythonOption *s);
};

static void
python_option_init_instance(PythonOption *s, const gchar *name)
{
  g_atomic_counter_set(&s->ref_cnt, 1);
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

PythonOption *
python_option_ref(PythonOption *s)
{
  g_assert(!s || g_atomic_counter_get(&s->ref_cnt) > 0);

  if (s)
    {
      g_atomic_counter_inc(&s->ref_cnt);
    }
  return s;
}

void
python_option_unref(PythonOption *s)
{
  g_assert(!s || g_atomic_counter_get(&s->ref_cnt));

  if (s && (g_atomic_counter_dec_and_test(&s->ref_cnt)))
    {
      if (s->free_fn)
        s->free_fn(s);

      g_free(s->name);
      g_free(s);
    }
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

/* String List */

typedef struct _PythonOptionStringList
{
  PythonOption super;
  GList *value;
} PythonOptionStringList;

static PyObject *
_string_list_create_value_py_object(const PythonOption *s)
{
  PythonOptionStringList *self = (PythonOptionStringList *) s;
  return py_string_list_from_string_list(self->value);
}

static void
_string_list_free_fn(PythonOption *s)
{
  PythonOptionStringList *self = (PythonOptionStringList *) s;
  string_list_free(self->value);
}

PythonOption *
python_option_string_list_new(const gchar *name, const GList *value)
{
  PythonOptionStringList *self = g_new0(PythonOptionStringList, 1);
  python_option_init_instance(&self->super, name);

  self->super.create_value_py_object = _string_list_create_value_py_object;
  self->super.free_fn = _string_list_free_fn;
  self->value = string_list_clone(value);

  return &self->super;
}

/* Template */

typedef struct _PythonOptionTemplate
{
  PythonOption super;
  gchar *value;
} PythonOptionTemplate;

static PyObject *
_template_create_value_py_object(const PythonOption *s)
{
  PythonOptionTemplate *self = (PythonOptionTemplate *) s;

  PyObject *template_str = py_string_from_string(self->value, -1);
  if (!template_str)
    return NULL;

  PyObject *args = PyTuple_Pack(1, template_str);
  PyObject *py_template = PyObject_Call((PyObject *) &py_log_template_type, args, NULL);

  Py_DECREF(template_str);
  Py_DECREF(args);
  return py_template;
}

static void
_template_free_fn(PythonOption *s)
{
  PythonOptionTemplate *self = (PythonOptionTemplate *) s;
  g_free(self->value);
}

PythonOption *
python_option_template_new(const gchar *name, const gchar *value)
{
  PythonOptionTemplate *self = g_new0(PythonOptionTemplate, 1);
  python_option_init_instance(&self->super, name);

  self->super.create_value_py_object = _template_create_value_py_object;
  self->super.free_fn = _template_free_fn;
  self->value = g_strdup(value);

  return &self->super;
}

/* Python Options */

struct _PythonOptions
{
  GList *options;
};

PythonOptions *
python_options_new(void)
{
  PythonOptions *self = g_new0(PythonOptions, 1);
  return self;
}

PythonOptions *
python_options_clone(const PythonOptions *self)
{
  PythonOptions *cloned = python_options_new();

  for (GList *elem = self->options; elem; elem = elem->next)
    python_options_add_option(cloned, (PythonOption *) elem->data);

  return cloned;
}

void
python_options_add_option(PythonOptions *self, PythonOption *option)
{
  self->options = g_list_append(self->options, python_option_ref(option));
}

PyObject *
python_options_create_py_dict(const PythonOptions *self)
{
  PyObject *py_dict;

  PyGILState_STATE gstate = PyGILState_Ensure();
  {
    py_dict = PyDict_New();
    if (!py_dict)
      {
        PyGILState_Release(gstate);
        return NULL;
      }

    for (GList *elem = self->options; elem; elem = elem->next)
      {
        const PythonOption *option = (const PythonOption *) elem->data;
        const gchar *name = python_option_get_name(option);
        PyObject *value = python_option_create_value_py_object(option);

        if (!value)
          continue;

        if (PyDict_SetItemString(py_dict, name, value) < 0)
          msg_error("python-options: Failed to add option to dict", evt_tag_str("name", name));
        Py_DECREF(value);
      }
  }
  PyGILState_Release(gstate);

  return py_dict;
}

void
python_options_free(PythonOptions *self)
{
  if (!self)
    return;

  g_list_free_full(self->options, (GDestroyNotify) python_option_unref);
  g_free(self);
}
