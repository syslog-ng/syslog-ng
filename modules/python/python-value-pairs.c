/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2014 Gergely Nagy <algernon@balabit.hu>
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
#include "python-value-pairs.h"
#include "python-helpers.h"
#include "messages.h"

/** Value pairs **/

static void
add_long_to_dict(PyObject *dict, const gchar *name, long num)
{
  gchar buf[256];

  PyObject *pyobject_to_add = PyLong_FromLong(num);
  if (!pyobject_to_add)
    {
      msg_error("Error while constructing python object",
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return;
    }

  PyDict_SetItemString(dict, name, pyobject_to_add);
  Py_DECREF(pyobject_to_add);
}

static void
add_string_to_dict(PyObject *dict, const gchar *name, const char *value, gsize value_len)
{
  gchar buf[256];

  PyObject *pyobject_to_add = PyBytes_FromStringAndSize(value, value_len);
  if (!pyobject_to_add)
    {
      msg_error("Error while constructing python object",
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();
      return;
    }

  PyDict_SetItemString(dict, name, pyobject_to_add);
  Py_DECREF(pyobject_to_add);
}

static gboolean
python_worker_vp_add_one(const gchar *name,
                         TypeHint type, const gchar *value, gsize value_len,
                         gpointer user_data)
{
  const LogTemplateOptions *template_options = (const LogTemplateOptions *)((gpointer *)user_data)[0];
  PyObject *dict = (PyObject *)((gpointer *)user_data)[1];
  gboolean need_drop = FALSE;
  gboolean fallback = template_options->on_error & ON_ERROR_FALLBACK_TO_STRING;

  switch (type)
    {
    case TYPE_HINT_INT32:
    case TYPE_HINT_INT64:
    {
      gint64 i;

      if (type_cast_to_int64(value, &i, NULL))
        {
          add_long_to_dict(dict, name, i);
        }
      else
        {
          need_drop = type_cast_drop_helper(template_options->on_error,
                                            value, "int");

          if (fallback)
            {
              add_string_to_dict(dict, name, value, value_len);
            }
        }
      break;
    }
    case TYPE_HINT_STRING:
      add_string_to_dict(dict, name, value, value_len);
      break;
    default:
      need_drop = type_cast_drop_helper(template_options->on_error,
                                        value, "<unknown>");
      break;
    }

  return need_drop;
}

/** Main code **/

gboolean
py_value_pairs_apply(ValuePairs *vp, const LogTemplateOptions *template_options, guint32 seq_num, LogMessage *msg,
                     PyObject **dict)
{
  gpointer args[2];
  gboolean vp_ok;

  *dict = PyDict_New();

  args[0] = (gpointer) template_options;
  args[1] = *dict;

  vp_ok = value_pairs_foreach(vp, python_worker_vp_add_one,
                              msg, seq_num, LTZ_LOCAL, template_options,
                              args);
  if (!vp_ok)
    {
      Py_DECREF(*dict);
      *dict = NULL;
    }
  return vp_ok;
}
